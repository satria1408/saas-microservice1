#include "meta_create.h"

#include <cctype>
#include <iostream>
#include <optional>
#include <stdexcept>

#include "alur/kemiripan.h"
#include "util.h"

std::string buat_key(const std::string& judul, const std::string& penulis) {
    const std::string gabungan = trim(huruf_kecil(judul + "_" + penulis));
    std::string out;
    bool pemisah_terakhir = false;
    for (unsigned char c : gabungan) {
        if (c < 128 && std::isalnum(c)) {
            out += static_cast<char>(c);
            pemisah_terakhir = false;
        } else if (!pemisah_terakhir) {
            out += '_';
            pemisah_terakhir = true;
        }
    }
    return out;
}

bool status_meta_valid(const std::string& s) {
    return s == "baru" || s == "ditinjau" || s == "dipromosikan" || s == "ditolak";
}

namespace {

// Lipat huruf besar -> kecil untuk huruf non-ASCII yang umum. normalisasi_kecil() sudah
// menangani ASCII dan Latin-1; ini menambah Latin Extended-A, Yunani, dan Sirilik dasar.
char32_t lipat(char32_t c) {
    if (c >= 0x100 && c <= 0x137) return (c % 2 == 0) ? c + 1 : c;
    if (c >= 0x139 && c <= 0x148) return (c % 2 == 1) ? c + 1 : c;
    if (c >= 0x14A && c <= 0x177) return (c % 2 == 0) ? c + 1 : c;
    if (c == 0x178) return 0xFF;
    if (c == 0x179 || c == 0x17B || c == 0x17D) return c + 1;
    if (c >= 0x391 && c <= 0x3A9 && c != 0x3A2) return c + 32;  // Yunani
    if (c >= 0x410 && c <= 0x42F) return c + 32;                // Sirilik U+0410-042F
    if (c >= 0x400 && c <= 0x40F) return c + 80;                // Sirilik U+0400-040F
    return c;
}

// Tanda baca dan simbol non-ASCII TIDAK dihitung sebagai huruf. Kalau dihitung, "Harry's" dan
// "Harry" + U+2019 + "s" (apostrof melengkung) dianggap dua buku padahal key-nya harus sama.
// Ini heuristik berbasis rentang; yang terlewat hanya berarti tabrakannya tidak terdeteksi
// (sama dengan perilaku sebelumnya), bukan salah tolak.
bool bukan_huruf(char32_t c) {
    return (c >= 0x80 && c <= 0xBF && c != 0xAA && c != 0xB5 && c != 0xBA)  // kontrol, NBSP, tanda baca Latin-1
        || c == 0xD7 || c == 0xF7                                           // x dan :
        || (c >= 0x2000 && c <= 0x2BFF)                                     // tanda baca umum, mata uang, panah, simbol
        || (c >= 0x3000 && c <= 0x303F)                                     // tanda baca CJK
        || (c >= 0xE000 && c <= 0xF8FF)                                     // area pribadi
        || (c >= 0xFE00 && c <= 0xFE6F) || c == 0xFEFF                      // variation selector, BOM
        || (c >= 0xFF01 && c <= 0xFF0F) || (c >= 0xFF1A && c <= 0xFF20)     // tanda baca fullwidth
        || (c >= 0xFF3B && c <= 0xFF40) || (c >= 0xFF5B && c <= 0xFF65)
        || (c >= 0x1F000 && c <= 0x1FAFF);                                  // emoji
}

}  // namespace

std::u32string tanda_huruf_asing(const std::string& utf8) {
    std::u32string hasil;
    for (char32_t c : normalisasi_kecil(utf8)) {
        if (c < 0x80) continue;
        c = lipat(c);
        if (!bukan_huruf(c)) hasil.push_back(c);
    }
    return hasil;
}

bool beda_huruf_asing(const std::string& judul_a, const std::string& penulis_a,
                      const std::string& judul_b, const std::string& penulis_b) {
    return tanda_huruf_asing(judul_a) != tanda_huruf_asing(judul_b) ||
           tanda_huruf_asing(penulis_a) != tanda_huruf_asing(penulis_b);
}

// Tambah satu kolom ke cache_metadata kalau belum ada (pola sama dengan db.cpp).
static void pastikan_kolom(sqlite3* db, const char* kolom, const char* tipe) {
    auto st = siapkan(db, "PRAGMA table_info(cache_metadata)");
    bool ada = false;
    while (sqlite3_step(st.get()) == SQLITE_ROW)
        if (kolom_teks(st.get(), 1) == kolom) ada = true;
    st.reset();  // lepas statement dulu sebelum ALTER
    if (!ada)
        jalankan(db, (std::string("ALTER TABLE cache_metadata ADD COLUMN ") + kolom + " " +
                      tipe).c_str());
}

void pastikan_skema_metadata(sqlite3* db) {
    jalankan(db,
        "CREATE TABLE IF NOT EXISTS cache_metadata ("
        " judul_penulis_key TEXT PRIMARY KEY,"
        " penerbit TEXT, sumber TEXT, isbn TEXT)");

    pastikan_kolom(db, "isbn", "TEXT");
    pastikan_kolom(db, "judul_asli", "TEXT");
    pastikan_kolom(db, "penulis_asli", "TEXT");
    pastikan_kolom(db, "status", "TEXT DEFAULT 'baru'");
    pastikan_kolom(db, "waktu_masuk", "TEXT");
    // Lindungi entri yang sudah dipromosikan dari INSERT berikutnya. Kalau tidak, entri
    jalankan(db,
        "CREATE TRIGGER IF NOT EXISTS cache_meta_lindungi BEFORE INSERT ON cache_metadata "
        "WHEN EXISTS (SELECT 1 FROM cache_metadata WHERE judul_penulis_key=NEW.judul_penulis_key "
        "AND status='dipromosikan') BEGIN SELECT RAISE(IGNORE); END");
}

HasilMeta meta_simpan(sqlite3* db, const std::string& judul, const std::string& penulis,
                      const std::string& penerbit, const std::string& isbn_mentah,
                      const std::string& sumber, std::string* penghuni) {
    if (trim(judul).empty()) return HasilMeta::Lewati;

    const std::string judul_b = trim(judul);
    const std::string key = buat_key(judul_b, trim(penulis));

    std::optional<std::string> isbn = bersihkan_isbn(isbn_mentah);
    if (isbn && !ean13_valid(*isbn)) {
        std::cerr << "[WARN] " << judul_b << ": ISBN '" << *isbn
                  << "' tidak lolos checksum, dikosongkan\n";
        isbn.reset();
    }
    std::optional<std::string> pener, penulis_b;
    if (!trim(penerbit).empty()) pener = trim(penerbit);
    if (!trim(penulis).empty()) penulis_b = trim(penulis);

    // Cek sudah ada atau belum, sekaligus ambil status dan judul/penulis aslinya.
    auto cari = siapkan(db,
        "SELECT COALESCE(status,'baru'), judul_asli, penulis_asli FROM cache_metadata"
        " WHERE judul_penulis_key=?");
    bind_teks(cari.get(), 1, key);
    int rc = sqlite3_step(cari.get());
    if (rc != SQLITE_ROW && rc != SQLITE_DONE) gagal(db, "cari");
    const bool ada = (rc == SQLITE_ROW);
    if (ada) {
        // Tabrakan key: buat_key() menyamakan semua huruf non-ASCII, jadi buku lain bisa
        // memakai key yang sama. Kalau huruf asingnya beda, ini BUKAN buku yang sama:
        // jangan timpa. Diperiksa SEBELUM status, supaya entri dipromosikan pun terlapor benar.
        // Baris lama dari notebook tidak punya judul_asli: tidak ada pembanding, jadi lolos.
        if (sqlite3_column_type(cari.get(), 1) != SQLITE_NULL) {
            const std::string j_lama = kolom_teks(cari.get(), 1);
            const std::string p_lama =
                sqlite3_column_type(cari.get(), 2) == SQLITE_NULL ? std::string() : kolom_teks(cari.get(), 2);
            if (beda_huruf_asing(j_lama, p_lama, judul_b, trim(penulis))) {
                if (penghuni) *penghuni = j_lama + " | " + p_lama;
                return HasilMeta::Tabrakan;
            }
        }
        // Entri yang sudah dipromosikan berisi data yang sudah diperiksa. Hasil scan berikutnya
        // tidak boleh menimpanya. Hanya 'cache promosi' yang boleh mengubahnya.
        if (kolom_teks(cari.get(), 0) == "dipromosikan") return HasilMeta::Lewati;
    }
    cari.reset();

    if (ada) {
        // COALESCE: nilai baru kosong -> nilai lama dipertahankan.
        // judul_asli/penulis_asli hanya diisi kalau masih kosong (baris lama dari notebook).
        // status sengaja TIDAK disentuh.
        auto upd = siapkan(db,
            "UPDATE cache_metadata SET penerbit=COALESCE(?,penerbit), isbn=COALESCE(?,isbn),"
            " sumber=?, judul_asli=COALESCE(judul_asli,?), penulis_asli=COALESCE(penulis_asli,?)"
            " WHERE judul_penulis_key=?");
        bind_opsional(upd.get(), 1, pener);
        bind_opsional(upd.get(), 2, isbn);
        bind_teks(upd.get(), 3, sumber);
        bind_teks(upd.get(), 4, judul_b);
        bind_opsional(upd.get(), 5, penulis_b);
        bind_teks(upd.get(), 6, key);
        if (sqlite3_step(upd.get()) != SQLITE_DONE) gagal(db, "update");
        return HasilMeta::Update;
    }

    auto ins = siapkan(db,
        "INSERT INTO cache_metadata (judul_penulis_key, penerbit, sumber, isbn,"
        " judul_asli, penulis_asli, status, waktu_masuk)"
        " VALUES (?,?,?,?,?,?,'baru',datetime('now'))");
    bind_teks(ins.get(), 1, key);
    bind_opsional(ins.get(), 2, pener);
    bind_teks(ins.get(), 3, sumber);
    bind_opsional(ins.get(), 4, isbn);
    bind_teks(ins.get(), 5, judul_b);
    bind_opsional(ins.get(), 6, penulis_b);
    if (sqlite3_step(ins.get()) != SQLITE_DONE) gagal(db, "insert");
    return HasilMeta::Baru;
}

void meta_tambah(sqlite3* db, const std::string& judul, const std::string& penulis,
                 const std::string& penerbit, const std::string& isbn) {
    if (trim(judul).empty()) throw std::runtime_error("cache add butuh minimal <judul>");
    std::string penghuni;
    HasilMeta h = meta_simpan(db, judul, penulis, penerbit, isbn, "input_manual", &penghuni);
    const std::string key = buat_key(trim(judul), trim(penulis));
    if (h == HasilMeta::Tabrakan)
        throw std::runtime_error("tabrakan key '" + key + "': sudah dipakai '" + penghuni +
                                 "', huruf non-ASCII-nya beda dari '" + trim(judul) + " | " +
                                 trim(penulis) + "'. Cache tidak ditimpa. Kalau entri lama itu "
                                 "yang salah: rag cache hapus " + key);
    if (h == HasilMeta::Lewati)
        std::cout << "[lewati] " << key << " sudah dipromosikan, cache tidak ditimpa\n";
    else
        std::cout << (h == HasilMeta::Baru ? "[baru]   " : "[update] ") << key << "\n";
}
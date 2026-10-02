#include "meta_create.h"

#include <cctype>
#include <iostream>
#include <optional>
#include <stdexcept>

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
}

HasilMeta meta_simpan(sqlite3* db, const std::string& judul, const std::string& penulis,
                      const std::string& penerbit, const std::string& isbn_mentah,
                      const std::string& sumber) {
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

    // cek sudah ada atau belum
    auto cari = siapkan(db, "SELECT 1 FROM cache_metadata WHERE judul_penulis_key=?");
    bind_teks(cari.get(), 1, key);
    int rc = sqlite3_step(cari.get());
    if (rc != SQLITE_ROW && rc != SQLITE_DONE) gagal(db, "cari");
    const bool ada = (rc == SQLITE_ROW);
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
    HasilMeta h = meta_simpan(db, judul, penulis, penerbit, isbn, "input_manual");
    std::cout << (h == HasilMeta::Baru ? "[baru]   " : "[update] ")
              << buat_key(trim(judul), trim(penulis)) << "\n";
}
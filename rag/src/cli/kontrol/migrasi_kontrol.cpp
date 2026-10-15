#include "kontrol/migrasi_kontrol.h"

#include <filesystem>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>

#include "cache/metadata/meta_create.h"
#include "cache/scan/scan_skema.h"
#include "db.h"
#include "katalog/katalog_skema.h"
#include "skema/skema_inti.h"    // db.cpp, katalog_skema.cpp
#include "skema/skema_cache.h"   // migrasi_kontrol.cpp
#include "util.h"

namespace {

namespace fs = std::filesystem;

std::string teks_atau_kosong(sqlite3_stmt* st, int i) {
    return sqlite3_column_type(st, i) == SQLITE_NULL ? std::string() : kolom_teks(st, i);
}

std::optional<std::string> atau_null(const std::string& s) {
    if (trim(s).empty()) return std::nullopt;
    return trim(s);
}

// Nama kolom kalau ada di tabel sumber, kalau tidak "NULL" (DB lama dari notebook mungkin
// belum punya semua kolom).
std::string kol(sqlite3* db, const char* tabel, const char* kolom) {
    return punya_kolom(db, tabel, kolom) ? std::string(kolom) : std::string("NULL");
}

long long hitung(sqlite3* db, const std::string& sql) {
    auto st = siapkan(db, sql.c_str());
    if (sqlite3_step(st.get()) != SQLITE_ROW) gagal(db, "hitung");
    return sqlite3_column_int64(st.get(), 0);
}

enum class StatusIsbn { Kosong, Valid, Tidak };

// Kosong = tidak ada ISBN (termasuk teks "null"/"None"). Tidak = ada isi tapi bukan ISBN-13.
StatusIsbn periksa_isbn(const std::string& mentah, std::string& bersih) {
    bersih.clear();
    if (trim(mentah).empty()) return StatusIsbn::Kosong;
    const auto b = bersihkan_isbn(mentah);
    if (!b) return StatusIsbn::Kosong;
    if (!isbn13_valid(*b)) return StatusIsbn::Tidak;
    bersih = *b;
    return StatusIsbn::Valid;
}

struct HasilRag {
    long long baca = 0, masuk = 0, tanpa_isbn = 0, isbn_dikosongkan = 0, kembar = 0, judul_kosong = 0;
};

// rag_manual lama -> inti.rag_manual. Aturan: id dipertahankan; ISBN yang bukan ISBN-13 valid
// dikosongkan (barisnya tetap pindah); kalau ISBN kembar, id terkecil menang dan sisanya dilewati.
HasilRag salin_rag(sqlite3* db, bool terapkan, bool rinci) {
    HasilRag h;
    if (!punya_tabel(db, "rag_manual")) return h;

    const std::string sql = "SELECT id, " + kol(db, "rag_manual", "judul") + ", " +
        kol(db, "rag_manual", "penulis") + ", " + kol(db, "rag_manual", "penerbit") + ", " +
        kol(db, "rag_manual", "isbn") + ", " + kol(db, "rag_manual", "sumber") + ", " +
        kol(db, "rag_manual", "waktu_masuk") + " FROM rag_manual ORDER BY id";
    auto baca = siapkan(db, sql.c_str());

    StmtPtr ins;
    if (terapkan)
        ins = siapkan(db,
            "INSERT INTO inti.rag_manual (id, judul, penulis, penerbit, isbn, sumber, waktu_masuk)"
            " VALUES (?1, ?2, ?3, ?4, ?5, COALESCE(?6,'input_manual'), COALESCE(?7,CURRENT_TIMESTAMP))");

    std::set<std::string> isbn_terpakai;
    int rc;
    while ((rc = sqlite3_step(baca.get())) == SQLITE_ROW) {
        ++h.baca;
        const long long id = sqlite3_column_int64(baca.get(), 0);
        const std::string judul = trim(teks_atau_kosong(baca.get(), 1));
        if (judul.empty()) {
            ++h.judul_kosong;
            if (rinci) std::cout << "  [rag_manual #" << id << "] dilewati: judul kosong\n";
            continue;
        }

        std::string isbn;
        std::optional<std::string> isbn_baru;
        switch (periksa_isbn(teks_atau_kosong(baca.get(), 4), isbn)) {
            case StatusIsbn::Valid: isbn_baru = isbn; break;
            case StatusIsbn::Tidak:
                ++h.isbn_dikosongkan;
                if (rinci)
                    std::cout << "  [rag_manual #" << id << "] ISBN '" << teks_atau_kosong(baca.get(), 4)
                              << "' dikosongkan (bukan ISBN-13 valid): " << judul << "\n";
                break;
            case StatusIsbn::Kosong: break;
        }
        if (isbn_baru) {
            if (!isbn_terpakai.insert(*isbn_baru).second) {
                ++h.kembar;
                if (rinci)
                    std::cout << "  [rag_manual #" << id << "] dilewati: ISBN " << *isbn_baru
                              << " sudah dipakai baris lain: " << judul << "\n";
                continue;
            }
        } else {
            ++h.tanpa_isbn;
        }

        if (terapkan) {
            sqlite3_reset(ins.get());
            sqlite3_clear_bindings(ins.get());
            sqlite3_bind_int64(ins.get(), 1, id);
            bind_teks(ins.get(), 2, judul);
            bind_opsional(ins.get(), 3, atau_null(teks_atau_kosong(baca.get(), 2)));
            bind_opsional(ins.get(), 4, atau_null(teks_atau_kosong(baca.get(), 3)));
            bind_opsional(ins.get(), 5, isbn_baru);
            bind_opsional(ins.get(), 6, atau_null(teks_atau_kosong(baca.get(), 5)));
            bind_opsional(ins.get(), 7, atau_null(teks_atau_kosong(baca.get(), 6)));
            if (sqlite3_step(ins.get()) != SQLITE_DONE) gagal(db, "salin rag_manual");
        }
        ++h.masuk;
    }
    if (rc != SQLITE_DONE) gagal(db, "baca rag_manual");
    return h;
}

struct HasilKatalog { long long baca = 0, masuk = 0; };

// katalog lama -> inti.katalog, apa adanya (id dipertahankan). Katalog sengaja longgar.
HasilKatalog salin_katalog(sqlite3* db, bool terapkan) {
    HasilKatalog h;
    if (!punya_tabel(db, "katalog")) return h;
    h.baca = hitung(db, "SELECT COUNT(*) FROM katalog");
    if (!terapkan) { h.masuk = h.baca; return h; }

    const std::string sql =
        "INSERT INTO inti.katalog (id, judul, penulis, penerbit, penerbit_sumber, stok,"
        " status_konfirmasi, waktu_masuk, isbn, kategori) SELECT id, " +
        kol(db, "katalog", "judul") + ", " + kol(db, "katalog", "penulis") + ", " +
        kol(db, "katalog", "penerbit") + ", " + kol(db, "katalog", "penerbit_sumber") + ", COALESCE(" +
        kol(db, "katalog", "stok") + ", 1), COALESCE(" + kol(db, "katalog", "status_konfirmasi") +
        ", 'otomatis'), COALESCE(" + kol(db, "katalog", "waktu_masuk") + ", CURRENT_TIMESTAMP), " +
        kol(db, "katalog", "isbn") + ", " + kol(db, "katalog", "kategori") + " FROM main.katalog";
    jalankan(db, sql.c_str());
    h.masuk = sqlite3_changes(db);
    return h;
}

struct HasilScan { long long baca = 0, masuk = 0; };

HasilScan salin_scan(sqlite3* db, bool terapkan) {
    HasilScan h;
    if (!punya_tabel(db, "cache_scan")) return h;
    h.baca = hitung(db, "SELECT COUNT(*) FROM cache_scan");
    if (!terapkan) { h.masuk = h.baca; return h; }

    const std::string sql =
        "INSERT OR IGNORE INTO cache.cache_scan (hash_gambar, judul, penulis, kategori, isbn, waktu_masuk)"
        " SELECT hash_gambar, " + kol(db, "cache_scan", "judul") + ", " + kol(db, "cache_scan", "penulis") +
        ", " + kol(db, "cache_scan", "kategori") + ", " + kol(db, "cache_scan", "isbn") +
        ", COALESCE(" + kol(db, "cache_scan", "waktu_masuk") + ", CURRENT_TIMESTAMP) FROM main.cache_scan";
    jalankan(db, sql.c_str());
    h.masuk = sqlite3_changes(db);
    return h;
}

struct HasilIsbn { long long baca = 0, masuk = 0, ditolak = 0, tanpa_isbn = 0, tanpa_penerbit = 0, kembar = 0; };

// cache_metadata lama -> cache.cache_isbn. Hanya entri yang punya ISBN-13 valid DAN penerbit,
// dan bukan berstatus 'ditolak'. Sisanya dibuang: cache boleh hilang (yang sudah dipromosikan
// datanya sudah ada di rag_manual).
HasilIsbn salin_cache_isbn(sqlite3* db, bool terapkan, bool rinci) {
    HasilIsbn h;
    if (!punya_tabel(db, "cache_metadata")) return h;

    const std::string sql = "SELECT judul_penulis_key, " + kol(db, "cache_metadata", "isbn") + ", " +
        kol(db, "cache_metadata", "judul_asli") + ", " + kol(db, "cache_metadata", "penulis_asli") + ", " +
        kol(db, "cache_metadata", "penerbit") + ", " + kol(db, "cache_metadata", "sumber") + ", " +
        kol(db, "cache_metadata", "status") + ", " + kol(db, "cache_metadata", "waktu_masuk") +
        " FROM cache_metadata ORDER BY COALESCE(" + kol(db, "cache_metadata", "waktu_masuk") + ",''), judul_penulis_key";
    auto baca = siapkan(db, sql.c_str());

    StmtPtr ins;
    if (terapkan)
        ins = siapkan(db,
            "INSERT INTO cache.cache_isbn (isbn, judul, penulis, penerbit, sumber, waktu_masuk)"
            " VALUES (?1, ?2, ?3, ?4, ?5, COALESCE(?6, CURRENT_TIMESTAMP))");

    std::set<std::string> terpakai;
    int rc;
    while ((rc = sqlite3_step(baca.get())) == SQLITE_ROW) {
        ++h.baca;
        const std::string key = kolom_teks(baca.get(), 0);
        if (teks_atau_kosong(baca.get(), 6) == "ditolak") { ++h.ditolak; continue; }

        std::string isbn;
        if (periksa_isbn(teks_atau_kosong(baca.get(), 1), isbn) != StatusIsbn::Valid) {
            ++h.tanpa_isbn;
            if (rinci) std::cout << "  [cache_metadata " << key << "] dibuang: tanpa ISBN-13 valid\n";
            continue;
        }
        const std::string penerbit = trim(teks_atau_kosong(baca.get(), 4));
        if (penerbit.empty()) {
            ++h.tanpa_penerbit;
            if (rinci) std::cout << "  [cache_metadata " << key << "] dibuang: penerbit kosong\n";
            continue;
        }
        if (!terpakai.insert(isbn).second) {
            ++h.kembar;
            if (rinci) std::cout << "  [cache_metadata " << key << "] dibuang: ISBN " << isbn << " sudah ada\n";
            continue;
        }
        if (terapkan) {
            sqlite3_reset(ins.get());
            sqlite3_clear_bindings(ins.get());
            bind_teks(ins.get(), 1, isbn);
            bind_opsional(ins.get(), 2, atau_null(teks_atau_kosong(baca.get(), 2)));
            bind_opsional(ins.get(), 3, atau_null(teks_atau_kosong(baca.get(), 3)));
            bind_teks(ins.get(), 4, penerbit);
            bind_opsional(ins.get(), 5, atau_null(teks_atau_kosong(baca.get(), 5)));
            bind_opsional(ins.get(), 6, atau_null(teks_atau_kosong(baca.get(), 7)));
            if (sqlite3_step(ins.get()) != SQLITE_DONE) gagal(db, "salin cache_isbn");
        }
        ++h.masuk;
    }
    if (rc != SQLITE_DONE) gagal(db, "baca cache_metadata");
    return h;
}

void lampirkan(sqlite3* db, const std::string& path, const char* nama) {
    auto st = siapkan(db, (std::string("ATTACH DATABASE ?1 AS ") + nama).c_str());
    bind_teks(st.get(), 1, path);
    if (sqlite3_step(st.get()) != SQLITE_DONE) gagal(db, std::string("attach ") + nama);
}

void cetak_laporan(const HasilRag& r, const HasilKatalog& k, const HasilScan& s, const HasilIsbn& c,
                   bool terapkan) {
    std::cout << "\ninti.db\n"
              << "  rag_manual : " << r.baca << " dibaca -> " << r.masuk << (terapkan ? " masuk" : " akan masuk") << "\n"
              << "      tanpa ISBN                          : " << r.tanpa_isbn << "\n"
              << "      ISBN dikosongkan (bukan ISBN-13)    : " << r.isbn_dikosongkan << "\n"
              << "      dilewati (ISBN kembar, id kecil menang): " << r.kembar << "\n"
              << "      dilewati (judul kosong)             : " << r.judul_kosong << "\n"
              << "  katalog    : " << k.baca << " dibaca -> " << k.masuk << (terapkan ? " masuk" : " akan masuk") << "\n"
              << "cache.db\n"
              << "  cache_scan : " << s.baca << " dibaca -> " << s.masuk << (terapkan ? " masuk" : " akan masuk") << "\n"
              << "  cache_isbn : " << c.baca << " baris cache_metadata dibaca -> " << c.masuk
              << (terapkan ? " masuk" : " akan masuk") << "\n"
              << "      dibuang (ditolak)        : " << c.ditolak << "\n"
              << "      dibuang (tanpa ISBN-13)  : " << c.tanpa_isbn << "\n"
              << "      dibuang (penerbit kosong): " << c.tanpa_penerbit << "\n"
              << "      dibuang (ISBN kembar)    : " << c.kembar << "\n";
}

// rag --db lama.db migrasi pisah                          -> laporan saja (tidak menulis apa pun)
// rag --db lama.db migrasi pisah <inti_baru.db> <cache_baru.db> -> buat dua file baru dan salin
// DB lama tidak pernah diubah, jadi tidak perlu backup.
void pisah(sqlite3* db, const Opsi& o) {
    const std::string inti = o.arg(2), cache = o.arg(3);
    const bool terapkan = !inti.empty() || !cache.empty();

    if (terapkan) {
        if (inti.empty() || cache.empty() || !o.arg(4).empty())
            throw std::runtime_error("migrasi pisah butuh tepat dua path: <inti_baru.db> <cache_baru.db>");
        if (inti == cache) throw std::runtime_error("path inti dan cache tidak boleh sama");
        for (const std::string& p : {inti, cache}) {
            std::error_code ec;
            if (fs::exists(p, ec) || fs::equivalent(p, o.db, ec))
                throw std::runtime_error("file sudah ada, tidak ditimpa: " + p);
        }
    }

    std::cout << (terapkan ? "Memisahkan DB: " : "Rencana pemisahan DB (CEK SAJA, tidak ada yang ditulis): ")
              << o.db << "\n";

    HasilRag r; HasilKatalog k; HasilScan s; HasilIsbn c;
    if (!terapkan) {
        r = salin_rag(db, false, o.rinci);
        k = salin_katalog(db, false);
        s = salin_scan(db, false);
        c = salin_cache_isbn(db, false, o.rinci);
        cetak_laporan(r, k, s, c, false);
        std::cout << "\nKalau angkanya sudah sesuai, jalankan:\n  rag --db " << o.db
                  << " migrasi pisah <inti_baru.db> <cache_baru.db>\n";
        if (!o.rinci) std::cout << "(tambahkan --rinci untuk melihat baris yang bermasalah)\n";
        return;
    }

    bool terlampir = false, mulai = false;
    try {
        { DbPtr d = buka_db(inti, ModeBuka::Buat);  pastikan_skema_inti(d.get()); }
        { DbPtr d = buka_db(cache, ModeBuka::Buat); pastikan_skema_cache(d.get()); }

        lampirkan(db, inti, "inti");
        lampirkan(db, cache, "cache");
        terlampir = true;

        jalankan(db, "BEGIN");  // dua file baru dalam mode DELETE: commit-nya atomik lintas file
        mulai = true;
        r = salin_rag(db, true, o.rinci);
        k = salin_katalog(db, true);
        s = salin_scan(db, true);
        c = salin_cache_isbn(db, true, o.rinci);
        jalankan(db, "COMMIT");
        mulai = false;

        jalankan(db, "DETACH DATABASE inti");
        jalankan(db, "DETACH DATABASE cache");
        terlampir = false;
    } catch (...) {
        if (mulai) sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        if (terlampir) {
            sqlite3_exec(db, "DETACH DATABASE inti", nullptr, nullptr, nullptr);
            sqlite3_exec(db, "DETACH DATABASE cache", nullptr, nullptr, nullptr);
        }
        std::error_code ec;  // dua file ini baru kita buat sendiri, aman dihapus
        fs::remove(inti, ec);
        fs::remove(cache, ec);
        throw;
    }

    cetak_laporan(r, k, s, c, true);
    std::cout << "\n[OK] selesai. DB lama tidak diubah. File baru: " << inti << " dan " << cache << "\n";
}

}  // namespace

void perintah_migrasi(sqlite3* db, const Opsi& o) {
    const std::string sub = o.arg(1);
    if (sub == "pisah") {
        pisah(db, o);
        return;
    }
    if (!sub.empty()) throw std::runtime_error("aksi migrasi tidak dikenal: " + sub);

    // Perilaku lama: mutakhirkan skema DB tunggal di tempat (masih dipakai selama masa transisi).
    if (!o.no_backup) buat_backup(db, o.db, o.simpan_backup);
    pastikan_skema(db);
    pastikan_skema_metadata(db);
    pastikan_skema_scan(db);
    pastikan_skema_katalog(db);
    std::cout << "[OK] skema rag_manual, cache_metadata, cache_scan, dan katalog sudah mutakhir\n";
}
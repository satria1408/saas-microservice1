#include "cache/scan/scan_skema.h"

#include <string>

#include "db.h"

namespace {

bool kolom_ada(sqlite3* db, const std::string& tabel, const std::string& kolom) {
    auto st = siapkan(db, ("PRAGMA table_info(" + tabel + ")").c_str());
    while (sqlite3_step(st.get()) == SQLITE_ROW)
        if (kolom_teks(st.get(), 1) == kolom) return true;
    return false;
}

void tambah_kolom_jika_belum(sqlite3* db, const std::string& tabel,
                             const std::string& kolom, const std::string& tipe) {
    if (kolom_ada(db, tabel, kolom)) return;
    const std::string sql = "ALTER TABLE " + tabel + " ADD COLUMN " + kolom + " " + tipe;
    jalankan(db, sql.c_str());
}

}  // namespace

void pastikan_skema_scan(sqlite3* db) {
    jalankan(db,
        "CREATE TABLE IF NOT EXISTS cache_scan ("
        " hash_gambar TEXT PRIMARY KEY,"
        " judul TEXT, penulis TEXT)");
    tambah_kolom_jika_belum(db, "cache_scan", "kategori", "TEXT");
    tambah_kolom_jika_belum(db, "cache_scan", "waktu_masuk", "TEXT");
    tambah_kolom_jika_belum(db, "cache_scan", "isbn", "TEXT");
    jalankan(db,
        "CREATE INDEX IF NOT EXISTS idx_scan_isbn ON cache_scan(isbn) "
        "WHERE isbn IS NOT NULL");
}
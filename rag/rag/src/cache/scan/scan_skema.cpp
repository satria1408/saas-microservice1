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
    // Tanpa DEFAULT: SQLite menolak default non-konstan (seperti CURRENT_TIMESTAMP)
    // pada ALTER TABLE. Baris lama jadi NULL, dan itu disengaja.
    tambah_kolom_jika_belum(db, "cache_scan", "waktu_masuk", "TEXT");
}
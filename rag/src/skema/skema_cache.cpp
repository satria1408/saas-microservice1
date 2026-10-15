#include "skema_cache.h"

#include "db.h"

void pastikan_skema_cache(sqlite3* db) {
    jalankan(db,
        "CREATE TABLE IF NOT EXISTS cache_scan ("
        " hash_gambar TEXT PRIMARY KEY,"
        " judul TEXT, penulis TEXT, kategori TEXT, isbn TEXT,"
        " waktu_masuk TEXT DEFAULT CURRENT_TIMESTAMP)");
    jalankan(db,
        "CREATE INDEX IF NOT EXISTS idx_scan_isbn ON cache_scan(isbn) WHERE isbn IS NOT NULL");
    jalankan(db,
        "CREATE TABLE IF NOT EXISTS cache_isbn ("
        " isbn TEXT PRIMARY KEY,"
        " judul TEXT, penulis TEXT, penerbit TEXT, sumber TEXT,"
        " waktu_masuk TEXT DEFAULT CURRENT_TIMESTAMP)");
    jalankan(db,
        "CREATE TABLE IF NOT EXISTS cache_kosong ("
        " isbn TEXT PRIMARY KEY,"
        " waktu_masuk TEXT DEFAULT CURRENT_TIMESTAMP)");
}
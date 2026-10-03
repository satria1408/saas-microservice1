#include "cache/scan/scan_skema.h"

#include "db.h"

void pastikan_skema_scan(sqlite3* db) {
    jalankan(db,
        "CREATE TABLE IF NOT EXISTS cache_scan ("
        " hash_gambar TEXT PRIMARY KEY,"
        " judul TEXT, penulis TEXT)");

    auto st = siapkan(db, "PRAGMA table_info(cache_scan)");
    bool ada_kategori = false;
    while (sqlite3_step(st.get()) == SQLITE_ROW)
        if (kolom_teks(st.get(), 1) == "kategori") ada_kategori = true;
    st.reset();
    if (!ada_kategori) jalankan(db, "ALTER TABLE cache_scan ADD COLUMN kategori TEXT");
}
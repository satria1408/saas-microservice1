#include "kontrol/migrasi_kontrol.h"

#include <iostream>

#include "cache/metadata/meta_create.h"
#include "cache/scan/scan_skema.h"
#include "db.h"
#include "katalog/katalog_skema.h"

void perintah_migrasi(sqlite3* db, const Opsi& o) {
    if (!o.no_backup) buat_backup(o.db, o.simpan_backup);
    pastikan_skema(db);
    pastikan_skema_metadata(db);
    pastikan_skema_scan(db);
    pastikan_skema_katalog(db);
    std::cout << "[OK] skema rag_manual, cache_metadata, cache_scan, dan katalog sudah mutakhir\n";
}
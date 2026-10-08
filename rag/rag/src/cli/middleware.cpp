#include "middleware.h"

#include <string>

#include "cache/metadata/meta_create.h"
#include "cache/scan/scan_skema.h"
#include "db.h"
#include "katalog/katalog_skema.h"

Akses tentukan_akses(const Opsi& o) {
    const std::string cmd = o.arg(0);
    const std::string sub = o.arg(1);
    const std::string aksi = o.arg(2);

    Akses a;
    a.menulis_katalog = (cmd == "katalog") && (sub == "konfirmasi");

    a.baca_saja = (cmd == "list") || (cmd == "cari") || (cmd == "cari-isbn") ||
        (cmd == "katalog" && !a.menulis_katalog) ||
        (cmd == "cache" && (sub == "list" ||
                            (sub == "scan" && (aksi == "list" || aksi == "get"))));

    a.menulis_cache = (cmd == "cache") &&
        (sub == "add" || sub == "status" || sub == "hapus" ||
         sub == "bersihkan" || sub == "promosi" ||
         (sub == "ttl" && (aksi == "pasang" || aksi == "lepas")) ||
         (sub == "scan" && (aksi == "add" || aksi == "hapus" || aksi == "bersihkan")));

    a.menulis_rag = (cmd == "add" || cmd == "import" || cmd == "edit" || cmd == "hapus");
    return a;
}

void jalankan_middleware(sqlite3* db, const Opsi& o) {
    const std::string cmd = o.arg(0);
    if (cmd == "migrasi") return;

    const Akses a = tentukan_akses(o);

    if (a.baca_saja) {
        jalankan(db, "PRAGMA query_only = ON");
        return;
    }

    if (a.menulis() && !o.no_backup) buat_backup(o.db, o.simpan_backup);

    pastikan_skema(db);
    if (cmd == "cache") {
        pastikan_skema_metadata(db);
        pastikan_skema_scan(db);
    }
    if (cmd == "katalog") pastikan_skema_katalog(db);
}
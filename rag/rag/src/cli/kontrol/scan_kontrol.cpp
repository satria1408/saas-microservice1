#include "kontrol/scan_kontrol.h"

#include <stdexcept>
#include <string>

#include "cache/scan/scan_bersihkan.h"
#include "cache/scan/scan_create.h"
#include "cache/scan/scan_hapus.h"
#include "cache/scan/scan_read.h"

void perintah_cache_scan(sqlite3* db, const Opsi& o) {
    const std::string aksi = o.arg(2);

    if (aksi == "list") {
        scan_daftar(db, o.arg(3));
    }
    else if (aksi == "get") {
        scan_tampil(db, wajib(o.arg(3), "cache scan get butuh <hash>"));
    }
    else if (aksi == "add") {
        scan_tambah(db, wajib(o.arg(3), "cache scan add butuh <hash> <judul> <penulis> [kategori]"),
                    o.arg(4), o.arg(5), o.arg(6));
    }
    else if (aksi == "hapus") {
        scan_hapus(db, wajib(o.arg(3), "cache scan hapus butuh <awalan-hash>"), o.yes);
    }
    else if (aksi == "bersihkan") {
        scan_bersihkan(db, o.hari, o.yes);
    }
    else if (aksi.empty()) throw std::runtime_error("cache scan butuh aksi: list, get, add, hapus, atau bersihkan");
    else throw std::runtime_error("aksi cache scan tidak dikenal: " + aksi);
}
#include "kontrol/cache_kontrol.h"

#include <stdexcept>
#include <string>

#include "cache/cache_ttl.h"
#include "cache/isbn/isbn_create.h"
#include "cache/isbn/isbn_hapus.h"
#include "cache/isbn/isbn_kosong.h"
#include "cache/isbn/isbn_read.h"
#include "kontrol/scan_kontrol.h"

// Semua perintah 'rag cache <sub> ...'. args[0]="cache", args[1]=sub. Bekerja di cache.db.
void perintah_cache(sqlite3* db, const Opsi& o) {
    const std::string sub = o.arg(1);

    if (sub == "list") {
        isbn_daftar(db, o.arg(2));
    }
    else if (sub == "get") {
        isbn_tampil(db, wajib(o.arg(2), "cache get butuh <isbn>"));
    }
    else if (sub == "add") {
        isbn_simpan(db, wajib(o.arg(2), "cache add butuh <isbn> <judul> [penulis] [penerbit] [sumber]"),
                    o.arg(3), o.arg(4), o.arg(5), o.arg(6));
    }
    else if (sub == "hapus") {
        isbn_hapus(db, wajib(o.arg(2), "cache hapus butuh <isbn>"), o.yes);
    }
    else if (sub == "kosong") {
        const std::string aksi = o.arg(2);
        if (aksi == "add") kosong_tambah(db, wajib(o.arg(3), "cache kosong add butuh <isbn>"));
        else if (aksi == "list") kosong_daftar(db);
        else if (aksi == "hapus") kosong_hapus(db, wajib(o.arg(3), "cache kosong hapus butuh <isbn>"));
        else if (aksi.empty()) throw std::runtime_error("cache kosong butuh aksi: add, list, atau hapus");
        else throw std::runtime_error("aksi cache kosong tidak dikenal: " + aksi);
    }
    else if (sub == "bersihkan") {
        cache_bersihkan(db, o.hari, o.yes);
    }
    else if (sub == "scan") {
        perintah_cache_scan(db, o);
    }
    else if (sub.empty()) throw std::runtime_error("cache butuh sub-perintah (jalankan rag tanpa argumen)");
    else throw std::runtime_error("sub-perintah cache tidak dikenal: " + sub);
}
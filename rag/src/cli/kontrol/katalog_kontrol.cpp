// katalog_kontrol.cpp
#include "kontrol/katalog_kontrol.h"

#include <stdexcept>
#include <string>
#include <vector>

#include "cache/isbn/isbn_hapus.h"
#include "katalog/katalog_read.h"
#include "katalog/katalog_tambah.h"
#include "katalog/katalog_ubah.h"

// Semua perintah 'rag katalog <aksi> ...'. args[0]="katalog", args[1]=aksi.
void perintah_katalog(sqlite3* db, sqlite3* cache, const Opsi& o) {
    const std::string aksi = o.arg(1);

    if (aksi == "list") {
        // Argumen pertama dianggap status kalau cocok otomatis/terkonfirmasi,
        // selain itu dianggap kata kunci.
        std::string status, kata;
        if (status_katalog_valid(o.arg(2))) { status = o.arg(2); kata = o.arg(3); }
        else kata = o.arg(2);
        katalog_daftar(db, status, kata);
    }
    else if (aksi == "get") {
        katalog_tampil(db, baca_id(wajib(o.arg(2), "katalog get butuh <id>"), "katalog get"));
    }
    else if (aksi == "tambah") {
        katalog_tambah(db,
            wajib(o.arg(2), "katalog tambah butuh <judul> [penulis] [penerbit] [isbn] [stok]"),
            o.arg(3), o.arg(4), o.arg(5), o.arg(6));
    }
    else if (aksi == "impor") {
        katalog_impor(db, wajib(o.arg(2), "katalog impor butuh <file.csv>"), o.rinci);
    }
    else if (aksi == "konfirmasi") {
        std::vector<std::string> isbn_masuk;
        katalog_konfirmasi(db,
            baca_id(wajib(o.arg(2), "katalog konfirmasi butuh <id>"), "katalog konfirmasi"),
            o.arg(3), o.arg(4), o.arg(5), o.yes, &isbn_masuk);
        bersihkan_cache_setelah_inti(cache, isbn_masuk);
    }
    else if (aksi.empty()) throw std::runtime_error("katalog butuh aksi: list, get, tambah, impor, atau konfirmasi");
    else throw std::runtime_error("aksi katalog tidak dikenal: " + aksi);
}
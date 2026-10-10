#include "kontrol/cache_kontrol.h"

#include <stdexcept>
#include <string>

#include "cache/metadata/meta_create.h"
#include "cache/metadata/meta_kelola.h"
#include "cache/metadata/meta_promosi.h"
#include "cache/metadata/meta_read.h"
#include "cache/metadata/meta_ttl.h"
#include "kontrol/scan_kontrol.h"

// Semua perintah 'rag cache <sub> ...'. args[0]="cache", args[1]=sub.
void perintah_cache(sqlite3* db, const Opsi& o) {
    const std::string sub = o.arg(1);

    if (sub == "list") {
        // Argumen pertama dianggap status kalau cocok salah satu status valid,
        // selain itu dianggap kata kunci.
        std::string status, kata;
        if (status_meta_valid(o.arg(2))) { status = o.arg(2); kata = o.arg(3); }
        else kata = o.arg(2);
        meta_daftar(db, status, kata);
    }
    else if (sub == "add") {  // hapus cabang ini kalau meta_tambah sudah kamu buang
        meta_tambah(db, o.arg(2), o.arg(3), o.arg(4), o.arg(5));
    }
    else if (sub == "status") {
        meta_set_status(db, wajib(o.arg(2), "cache status butuh <key> <status>"),
                        wajib(o.arg(3), "cache status butuh <key> <status>"));
    }
    else if (sub == "hapus") {
        meta_hapus(db, wajib(o.arg(2), "cache hapus butuh <key>"), o.yes);
    }
    else if (sub == "bersihkan") {
        meta_bersihkan(db, o.hari, o.yes);
    }
    else if (sub == "ttl") {
        const std::string aksi = o.arg(2);
        if (aksi.empty()) meta_status_ttl(db);
        else if (aksi == "pasang") {
            int ditolak = o.arg(3).empty() ? 7  : baca_angka(o.arg(3), "hari ditolak");
            int promosi = o.arg(4).empty() ? 0  : baca_angka(o.arg(4), "hari dipromosikan");
            int baru    = o.arg(5).empty() ? 90 : baca_angka(o.arg(5), "hari baru");
            meta_pasang_ttl(db, ditolak, promosi, baru);
        }
        else if (aksi == "lepas") meta_lepas_ttl(db);
        else throw std::runtime_error("cache ttl: aksi harus pasang atau lepas");
    }
    else if (sub == "promosi") {
        meta_promosi(db, wajib(o.arg(2), "cache promosi butuh <key>"), o.arg(3), o.arg(4));
    }
    else if (sub == "scan") {
        perintah_cache_scan(db, o);
    }
    else if (sub.empty()) throw std::runtime_error("cache butuh sub-perintah (jalankan rag tanpa argumen)");
    else throw std::runtime_error("sub-perintah cache tidak dikenal: " + sub);
}
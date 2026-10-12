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
        ttl_bersihkan(db, o.hari, o.yes, [&o] {
            if (!o.no_backup) buat_backup(o.db, o.simpan_backup);
        });
    }
    else if (sub == "ttl") {
        const std::string aksi = o.arg(2);
        if (aksi.empty()) ttl_status(db);
        else if (aksi == "pasang") {
            AturanTtl a; 
            if (!o.arg(3).empty()) a.meta_ditolak       = baca_angka(o.arg(3), "hari ditolak");
            if (!o.arg(4).empty()) a.meta_dipromosikan  = baca_angka(o.arg(4), "hari dipromosikan");
            if (!o.arg(5).empty()) a.meta_baru          = baca_angka(o.arg(5), "hari baru");
            if (!o.arg(6).empty()) a.scan               = baca_angka(o.arg(6), "hari scan");
            if (!o.arg(7).empty()) a.katalog_otomatis   = baca_angka(o.arg(7), "hari katalog");
            ttl_pasang(db, a);
        }
        else if (aksi == "lepas") ttl_lepas(db);
        else throw std::runtime_error("cache ttl: aksi harus pasang atau lepas");
    }
    else if (sub == "promosi") {
        if (o.otomatis)
            meta_promosi_otomatis(db, o.cek, o.rinci, [&o] {
                if (!o.no_backup) buat_backup(o.db, o.simpan_backup);
            });
        else meta_promosi(db, wajib(o.arg(2), "cache promosi butuh <key>"), o.arg(3), o.arg(4));
    }
    else if (sub == "scan") {
        perintah_cache_scan(db, o);
    }
    else if (sub.empty()) throw std::runtime_error("cache butuh sub-perintah (jalankan rag tanpa argumen)");
    else throw std::runtime_error("sub-perintah cache tidak dikenal: " + sub);
}
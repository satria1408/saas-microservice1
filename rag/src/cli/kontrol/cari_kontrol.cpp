#include "kontrol/cari_kontrol.h"

#include "alur/alur_cari.h"

void perintah_cari(sqlite3* inti, sqlite3* cache, const Opsi& o) {
    cetak_hasil(cari_judul(inti, cache,
        wajib(o.arg(1), "cari butuh <judul> [penulis]"),
        o.arg(2)));
}

void perintah_cari_isbn(sqlite3* inti, sqlite3* cache, const Opsi& o) {
    cetak_hasil(cari_isbn(inti, cache, wajib(o.arg(1), "cari-isbn butuh <isbn>")));
}

void perintah_cari_hash(sqlite3* inti, sqlite3* cache, const Opsi& o) {
    cetak_hasil(cari_hash(inti, cache, wajib(o.arg(1), "cari-hash butuh <hash atau awalannya>")));
}
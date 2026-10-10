#include "kontrol/cari_kontrol.h"

#include "alur/alur_cari.h"

void perintah_cari(sqlite3* db, const Opsi& o) {
    cetak_hasil(cari_metadata(db,
        wajib(o.arg(1), "cari butuh <judul> [penulis]"),
        o.arg(2)));
}

void perintah_cari_isbn(sqlite3* db, const Opsi& o) {
    cetak_hasil(cari_dari_isbn_lokal(db,
        wajib(o.arg(1), "cari-isbn butuh <isbn>")));
}
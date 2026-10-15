#include "router.h"

#include <stdexcept>
#include <string>

#include "kontrol/cache_kontrol.h"
#include "kontrol/cari_kontrol.h"
#include "kontrol/katalog_kontrol.h"
#include "kontrol/migrasi_kontrol.h"
#include "kontrol/rag_kontrol.h"
#include "kontrol/wal_kontrol.h"

void arahkan(Koneksi& k, const Opsi& o) {
    const std::string cmd = o.arg(0);
    sqlite3* inti = k.inti.get();
    sqlite3* cache = k.cache.get();

    if (cmd == "migrasi")        perintah_migrasi(inti, o);
    else if (cmd == "cari")      perintah_cari(inti, cache, o);
    else if (cmd == "cari-isbn") perintah_cari_isbn(inti, cache, o);
    else if (cmd == "cari-hash") perintah_cari_hash(inti, cache, o);
    else if (cmd == "katalog")   perintah_katalog(inti, cache, o);
    else if (cmd == "cache")     perintah_cache(cache, o);
    else if (cmd == "wal")       perintah_wal(inti, o);
    else if (cmd == "add" || cmd == "import" || cmd == "list" ||
             cmd == "edit" || cmd == "hapus")
                                 perintah_rag(inti, cache, o);
    else throw std::runtime_error("perintah tidak dikenal: " + cmd);
}
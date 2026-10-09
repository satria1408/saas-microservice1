#include "router.h"

#include <stdexcept>
#include <string>

#include "kontrol/cache_kontrol.h"
#include "kontrol/cari_kontrol.h"
#include "kontrol/katalog_kontrol.h"
#include "kontrol/migrasi_kontrol.h"
#include "kontrol/rag_kontrol.h"

void arahkan(sqlite3* db, const Opsi& o) {
    const std::string cmd = o.arg(0);

    if (cmd == "migrasi")        perintah_migrasi(db, o);
    else if (cmd == "cari")      perintah_cari(db, o);
    else if (cmd == "cari-isbn") perintah_cari_isbn(db, o);
    else if (cmd == "katalog")   perintah_katalog(db, o);
    else if (cmd == "cache")     perintah_cache(db, o);
    else if (cmd == "add" || cmd == "import" || cmd == "list" ||
             cmd == "edit" || cmd == "hapus")
                                 perintah_rag(db, o);
    else throw std::runtime_error("perintah tidak dikenal: " + cmd);
}
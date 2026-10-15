// rag_kontrol.cpp
#include "kontrol/rag_kontrol.h"

#include <stdexcept>
#include <string>
#include <vector>

#include "cache/isbn/isbn_hapus.h"
#include "crud/create.h"
#include "crud/delete.h"
#include "crud/read.h"
#include "crud/update.h"

void perintah_rag(sqlite3* db, sqlite3* cache, const Opsi& o) {
    const std::string cmd = o.arg(0);
    std::vector<std::string> isbn_masuk;  // ISBN yang berhasil tersimpan di inti

    if (cmd == "add") {
        tambah(db, o.arg(1), o.arg(2), o.arg(3), o.arg(4), &isbn_masuk);
    }
    else if (cmd == "import") {
        // Cek di validasi_bentuk tetap ada; ini jaring kedua.
        if (o.arg(1).empty()) throw std::runtime_error("import butuh <file.csv>");
        impor(db, o.arg(1), !o.tanpa_transaksi, o.rinci, &isbn_masuk);
    }
    else if (cmd == "list") {
        daftar(db, o.arg(1));
    }
    else if (cmd == "edit") {
        edit(db, baca_id(o.arg(1), "edit"), o.arg(2), o.arg(3), &isbn_masuk);
    }
    else if (cmd == "hapus") {
        hapus(db, baca_id(o.arg(1), "hapus"), o.yes);
    }
    else {
        throw std::runtime_error("perintah tidak dikenal: " + cmd);
    }

    bersihkan_cache_setelah_inti(cache, isbn_masuk);  // setelah inti beres, best-effort
}
#include "kontrol/rag_kontrol.h"

#include <stdexcept>
#include <string>

#include "crud/create.h"
#include "crud/delete.h"
#include "crud/read.h"
#include "crud/update.h"

void perintah_rag(sqlite3* db, const Opsi& o) {
    const std::string cmd = o.arg(0);

    if (cmd == "add") {
        tambah(db, o.arg(1), o.arg(2), o.arg(3), o.arg(4));
    }
    else if (cmd == "import") {
        // Cek di validasi_bentuk tetap ada; ini jaring kedua.
        if (o.arg(1).empty()) throw std::runtime_error("import butuh <file.csv>");
        impor(db, o.arg(1), !o.tanpa_transaksi, o.rinci);
    }
    else if (cmd == "list") {
        daftar(db, o.arg(1));
    }
    else if (cmd == "edit") {
        edit(db, baca_id(o.arg(1), "edit"), o.arg(2), o.arg(3));
    }
    else if (cmd == "hapus") {
        hapus(db, baca_id(o.arg(1), "hapus"), o.yes);
    }
    else {
        throw std::runtime_error("perintah tidak dikenal: " + cmd);
    }
}
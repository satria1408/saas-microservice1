#include "update.h"

#include <iostream>
#include <optional>
#include <stdexcept>

#include "util.h"

void edit(sqlite3* db, long long id, const std::string& field, const std::string& nilai) {
    // Nama kolom tidak bisa di-bind pakai '?', jadi dicek manual (whitelist).
    // Ini juga yang mencegah SQL disisipkan lewat argumen.
    if (field != "judul" && field != "penulis" && field != "penerbit" && field != "isbn")
        throw std::runtime_error("kolom harus: judul, penulis, penerbit, atau isbn");

    std::optional<std::string> isi;
    if (field == "isbn") {
        isi = bersihkan_isbn(nilai);
        if (isi && !ean13_valid(*isi)) throw std::runtime_error("ISBN tidak lolos checksum");
    } else if (!trim(nilai).empty()) {
        isi = trim(nilai);
    }
    if (field == "judul" && !isi) throw std::runtime_error("judul tidak boleh kosong");

    std::string sql = "UPDATE rag_manual SET " + field + "=? WHERE id=?";
    auto st = siapkan(db, sql.c_str());
    bind_opsional(st.get(), 1, isi);
    sqlite3_bind_int64(st.get(), 2, id);
    if (sqlite3_step(st.get()) != SQLITE_DONE) gagal(db, "edit");

    if (sqlite3_changes(db) == 0) std::cout << "ID " << id << " tidak ada.\n";
    else std::cout << "Terupdate #" << id << ": " << field << "\n";
}

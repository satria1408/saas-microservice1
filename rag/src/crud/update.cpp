#include "update.h"

#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include "util.h"

void edit(sqlite3* db, long long id, const std::string& field, const std::string& nilai,
          std::vector<std::string>* isbn_masuk) {
    // Nama kolom tidak bisa di-bind pakai '?', jadi dicek manual (whitelist).
    // Ini juga yang mencegah SQL disisipkan lewat argumen.
    if (field != "judul" && field != "penulis" && field != "penerbit" && field != "isbn")
        throw std::runtime_error("kolom harus: judul, penulis, penerbit, atau isbn");

    std::optional<std::string> isi;
    if (field == "isbn") {
        isi = bersihkan_isbn(nilai);  // kosong = mengosongkan ISBN
        if (isi && !isbn13_valid(*isi))
            throw std::runtime_error("ISBN bukan ISBN-13 yang valid (awalan 978/979 dan checksum benar)");
    } else if (!trim(nilai).empty()) {
        isi = trim(nilai);
    }
    if (field == "judul" && !isi) throw std::runtime_error("judul tidak boleh kosong");

    std::string sql = "UPDATE rag_manual SET " + field + "=? WHERE id=?";
    auto st = siapkan(db, sql.c_str());
    bind_opsional(st.get(), 1, isi);
    sqlite3_bind_int64(st.get(), 2, id);
    const int rc = sqlite3_step(st.get());

    if (rc == SQLITE_CONSTRAINT && field == "isbn" && isi) {
        st.reset();
        auto pemilik = siapkan(db, "SELECT id FROM rag_manual WHERE isbn=?");
        bind_teks(pemilik.get(), 1, *isi);
        if (sqlite3_step(pemilik.get()) == SQLITE_ROW)
            throw std::runtime_error("ISBN " + *isi + " sudah dipakai buku #" +
                                     std::to_string(sqlite3_column_int64(pemilik.get(), 0)));
    }
    if (rc != SQLITE_DONE) gagal(db, "edit");

    if (sqlite3_changes(db) == 0) std::cout << "ID " << id << " tidak ada.\n";
    else {
        std::cout << "Terupdate #" << id << ": " << field << "\n";
        if (isbn_masuk && field == "isbn" && isi) isbn_masuk->push_back(*isi);
    }
}
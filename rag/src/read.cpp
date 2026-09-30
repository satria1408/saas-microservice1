#include "read.h"

#include <iostream>

#include "util.h"

void daftar(sqlite3* db, const std::string& kata) {
    std::string sql = "SELECT id, judul, penulis, penerbit, isbn, sumber FROM rag_manual";
    if (!kata.empty())
        sql += " WHERE LOWER(judul) LIKE ?1 OR LOWER(penulis) LIKE ?1"
               " OR LOWER(penerbit) LIKE ?1 OR isbn LIKE ?1";
    sql += " ORDER BY id";

    auto st = siapkan(db, sql.c_str());
    if (!kata.empty()) bind_teks(st.get(), 1, "%" + huruf_kecil(kata) + "%");

    int n = 0, rc;
    while ((rc = sqlite3_step(st.get())) == SQLITE_ROW) {
        std::cout << "#" << sqlite3_column_int64(st.get(), 0) << " "
                  << kolom_teks(st.get(), 1) << " | " << kolom_teks(st.get(), 2) << " | "
                  << kolom_teks(st.get(), 3) << " | ISBN: " << kolom_teks(st.get(), 4) << " | "
                  << kolom_teks(st.get(), 5) << "\n";
        ++n;
    }
    if (rc != SQLITE_DONE) gagal(db, "membaca baris");
    std::cout << "\n" << n << " baris.\n";
}

#include "cache/isbn/isbn_read.h"

#include <iostream>
#include <stdexcept>

#include "db.h"
#include "util.h"

namespace {
std::string teks_atau_kosong(sqlite3_stmt* st, int i) {
    return sqlite3_column_type(st, i) == SQLITE_NULL ? std::string() : kolom_teks(st, i);
}
std::string tampil(const std::string& s) { return s.empty() ? std::string("-") : s; }
}  // namespace

void isbn_daftar(sqlite3* db, const std::string& kata) {
    auto st = siapkan(db,
        "SELECT isbn, judul, penulis, penerbit FROM cache_isbn "
        "WHERE (?1 = '' OR isbn LIKE '%' || ?1 || '%' OR judul LIKE '%' || ?1 || '%' "
        "OR penulis LIKE '%' || ?1 || '%' OR penerbit LIKE '%' || ?1 || '%') "
        "ORDER BY judul, isbn");
    bind_teks(st.get(), 1, kata);

    int n = 0, rc;
    while ((rc = sqlite3_step(st.get())) == SQLITE_ROW) {
        std::cout << teks_atau_kosong(st.get(), 0) << " | " << tampil(teks_atau_kosong(st.get(), 1))
                  << " | " << tampil(teks_atau_kosong(st.get(), 2)) << " | "
                  << tampil(teks_atau_kosong(st.get(), 3)) << "\n";
        ++n;
    }
    if (rc != SQLITE_DONE) gagal(db, "baca cache_isbn");
    std::cout << n << " baris\n";
}

void isbn_tampil(sqlite3* db, const std::string& isbn_mentah) {
    const auto isbn = bersihkan_isbn(isbn_mentah);
    if (!isbn) throw std::runtime_error("isbn kosong");

    bool ada = false;
    {
        auto st = siapkan(db,
            "SELECT judul, penulis, penerbit, sumber, waktu_masuk FROM cache_isbn WHERE isbn = ?");
        bind_teks(st.get(), 1, *isbn);
        const int rc = sqlite3_step(st.get());
        if (rc == SQLITE_ROW) {
            ada = true;
            std::cout << "isbn     : " << *isbn << "\n"
                      << "judul    : " << tampil(teks_atau_kosong(st.get(), 0)) << "\n"
                      << "penulis  : " << tampil(teks_atau_kosong(st.get(), 1)) << "\n"
                      << "penerbit : " << tampil(teks_atau_kosong(st.get(), 2)) << "\n"
                      << "sumber   : " << tampil(teks_atau_kosong(st.get(), 3)) << "\n"
                      << "waktu    : " << tampil(teks_atau_kosong(st.get(), 4)) << "\n";
        } else if (rc != SQLITE_DONE) {
            gagal(db, "baca cache_isbn");
        }
    }
    auto st = siapkan(db, "SELECT waktu_masuk FROM cache_kosong WHERE isbn = ?");
    bind_teks(st.get(), 1, *isbn);
    const int rc = sqlite3_step(st.get());
    if (rc == SQLITE_ROW) {
        ada = true;
        std::cout << "kosong   : ya, tercatat tidak ketemu sejak " << teks_atau_kosong(st.get(), 0) << "\n";
    } else if (rc != SQLITE_DONE) {
        gagal(db, "baca cache_kosong");
    }
    if (!ada) throw std::runtime_error("ISBN tidak ada di cache_isbn maupun cache_kosong: " + *isbn);
}
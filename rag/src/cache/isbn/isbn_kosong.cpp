#include "cache/isbn/isbn_kosong.h"

#include <iostream>
#include <stdexcept>

#include "db.h"
#include "util.h"

namespace {
std::string isbn_valid_atau_lempar(const std::string& mentah) {
    const auto isbn = bersihkan_isbn(mentah);
    if (!isbn || !isbn13_valid(*isbn))
        throw std::runtime_error("ISBN '" + trim(mentah) +
                                 "' bukan ISBN-13 yang valid (awalan 978/979 dan checksum benar)");
    return *isbn;
}
}  // namespace

void kosong_tambah(sqlite3* db, const std::string& isbn_mentah) {
    const std::string isbn = isbn_valid_atau_lempar(isbn_mentah);
    {
        auto cek = siapkan(db, "SELECT 1 FROM cache_isbn WHERE isbn = ?");
        bind_teks(cek.get(), 1, isbn);
        if (sqlite3_step(cek.get()) == SQLITE_ROW)
            throw std::runtime_error("ISBN " + isbn + " sudah ada di cache_isbn; hapus dulu "
                                     "(cache hapus) kalau memang mau dicatat sebagai kosong");
    }
    auto st = siapkan(db,
        "INSERT INTO cache_kosong (isbn, waktu_masuk) VALUES (?1, datetime('now')) "
        "ON CONFLICT(isbn) DO UPDATE SET waktu_masuk = datetime('now')");
    bind_teks(st.get(), 1, isbn);
    if (sqlite3_step(st.get()) != SQLITE_DONE) gagal(db, "simpan cache_kosong");
    std::cout << "[OK] tercatat kosong: " << isbn << "\n";
}

void kosong_daftar(sqlite3* db) {
    auto st = siapkan(db, "SELECT isbn, waktu_masuk FROM cache_kosong ORDER BY waktu_masuk DESC, isbn");
    int n = 0, rc;
    while ((rc = sqlite3_step(st.get())) == SQLITE_ROW) {
        std::cout << kolom_teks(st.get(), 0) << " | " << kolom_teks(st.get(), 1) << "\n";
        ++n;
    }
    if (rc != SQLITE_DONE) gagal(db, "baca cache_kosong");
    std::cout << n << " baris\n";
}

void kosong_hapus(sqlite3* db, const std::string& isbn_mentah) {
    const std::string isbn = isbn_valid_atau_lempar(isbn_mentah);
    auto st = siapkan(db, "DELETE FROM cache_kosong WHERE isbn = ?");
    bind_teks(st.get(), 1, isbn);
    if (sqlite3_step(st.get()) != SQLITE_DONE) gagal(db, "hapus cache_kosong");
    if (sqlite3_changes(db) == 0) std::cout << "ISBN " << isbn << " tidak ada di cache_kosong.\n";
    else std::cout << "[OK] dihapus dari cache_kosong: " << isbn << "\n";
}
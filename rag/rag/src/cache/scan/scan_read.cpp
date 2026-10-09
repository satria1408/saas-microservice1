#include "cache/scan/scan_read.h"

#include <iostream>
#include <stdexcept>

#include "cache/scan/scan_model.h"
#include "db.h"
#include "util.h"

void scan_daftar(sqlite3* db, const std::string& kata) {
    auto st = siapkan(db,
        "SELECT hash_gambar, judul, penulis, kategori FROM cache_scan "
        "WHERE (?1 = '' OR judul LIKE '%' || ?1 || '%' OR penulis LIKE '%' || ?1 || '%') "
        "ORDER BY judul");
    bind_teks(st.get(), 1, kata);

    int n = 0, rc;
    while ((rc = sqlite3_step(st.get())) == SQLITE_ROW) {
        std::string hash = kolom_teks(st.get(), 0);
        if (hash.size() > 12) hash.resize(12);
        std::cout << hash << " | " << kolom_teks(st.get(), 1) << " | "
                  << kolom_teks(st.get(), 2) << " | " << kolom_teks(st.get(), 3) << "\n";
        ++n;
    }
    if (rc != SQLITE_DONE) gagal(db, "baca cache_scan");
    std::cout << n << " baris\n";
}

void scan_tampil(sqlite3* db, const std::string& awalan_hash) {
    // Range di kolom aslinya (bukan substr), jadi index primary key kepakai.
    auto st = siapkan(db,
        "SELECT hash_gambar, judul, penulis, kategori, waktu_masuk, isbn FROM cache_scan "
        "WHERE hash_gambar >= ?1 AND hash_gambar < ?2 LIMIT 2");
    bind_teks(st.get(), 1, awalan_hash);
    bind_teks(st.get(), 2, batas_atas_awalan(awalan_hash));

    int rc = sqlite3_step(st.get());
    if (rc == SQLITE_DONE) throw std::runtime_error("hash tidak ditemukan: " + awalan_hash);
    if (rc != SQLITE_ROW) gagal(db, "baca cache_scan");

    ScanCache s{kolom_teks(st.get(), 0), kolom_teks(st.get(), 1),
                kolom_teks(st.get(), 2), kolom_teks(st.get(), 3),
                kolom_teks(st.get(), 5)};
    const std::string waktu = kolom_teks(st.get(), 4);

    rc = sqlite3_step(st.get());
    if (rc == SQLITE_ROW)
        throw std::runtime_error("awalan hash ambigu, cocok lebih dari satu: " + awalan_hash);
    if (rc != SQLITE_DONE) gagal(db, "baca cache_scan");

    std::cout << "hash     : " << s.hash << "\n"
              << "judul    : " << s.judul << "\n"
              << "penulis  : " << s.penulis << "\n"
              << "kategori : " << s.kategori << "\n"
              << "isbn     : " << s.isbn << "\n"
              << "waktu    : " << waktu << "\n";
}

void scan_cari_isbn(sqlite3* db, const std::string& isbn_mentah) {
    const auto isbn = bersihkan_isbn(isbn_mentah);
    if (!isbn) throw std::runtime_error("isbn kosong");

    auto st = siapkan(db,
        "SELECT hash_gambar, judul, penulis, kategori FROM cache_scan "
        "WHERE isbn = ?1 ORDER BY waktu_masuk DESC, hash_gambar");
    bind_teks(st.get(), 1, *isbn);

    int n = 0, rc;
    while ((rc = sqlite3_step(st.get())) == SQLITE_ROW) {
        std::string hash = kolom_teks(st.get(), 0);
        if (hash.size() > 12) hash.resize(12);
        std::cout << hash << " | " << kolom_teks(st.get(), 1) << " | "
                  << kolom_teks(st.get(), 2) << " | " << kolom_teks(st.get(), 3) << "\n";
        ++n;
    }
    if (rc != SQLITE_DONE) gagal(db, "baca cache_scan");
    std::cout << n << " hash untuk ISBN " << *isbn << "\n";
}
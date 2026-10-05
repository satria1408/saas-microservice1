#include "cache/scan/scan_read.h"

#include <iostream>
#include <stdexcept>

#include "cache/scan/scan_model.h"
#include "db.h"

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
    auto st = siapkan(db,
        "SELECT hash_gambar, judul, penulis, kategori, waktu_masuk FROM cache_scan "
        "WHERE substr(hash_gambar, 1, length(?1)) = ?1 LIMIT 2");
    bind_teks(st.get(), 1, awalan_hash);

    int rc = sqlite3_step(st.get());
    if (rc == SQLITE_DONE) throw std::runtime_error("hash tidak ditemukan: " + awalan_hash);
    if (rc != SQLITE_ROW) gagal(db, "baca cache_scan");

    ScanCache s{kolom_teks(st.get(), 0), kolom_teks(st.get(), 1),
                kolom_teks(st.get(), 2), kolom_teks(st.get(), 3)};
    const std::string waktu = kolom_teks(st.get(), 4);

    rc = sqlite3_step(st.get());
    if (rc == SQLITE_ROW)
        throw std::runtime_error("awalan hash ambigu, cocok lebih dari satu: " + awalan_hash);
    if (rc != SQLITE_DONE) gagal(db, "baca cache_scan");

    std::cout << "hash     : " << s.hash << "\n"
              << "judul    : " << s.judul << "\n"
              << "penulis  : " << s.penulis << "\n"
              << "kategori : " << s.kategori << "\n"
              << "waktu    : " << waktu << "\n";
}
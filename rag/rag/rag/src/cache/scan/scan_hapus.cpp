#include "cache/scan/scan_hapus.h"

#include <iostream>
#include <stdexcept>

#include "cache/scan/scan_model.h"
#include "db.h"
#include "util.h"

void scan_hapus(sqlite3* db, const std::string& awalan_hash, bool yes) {
    if (awalan_hash.empty()) throw std::runtime_error("awalan hash tidak boleh kosong");

    auto cari = siapkan(db,
        "SELECT hash_gambar, judul, penulis, kategori, isbn FROM cache_scan "
        "WHERE hash_gambar >= ?1 AND hash_gambar < ?2 LIMIT 2");
    bind_teks(cari.get(), 1, awalan_hash);
    bind_teks(cari.get(), 2, batas_atas_awalan(awalan_hash));

    int rc = sqlite3_step(cari.get());
    if (rc == SQLITE_DONE) throw std::runtime_error("hash tidak ditemukan: " + awalan_hash);
    if (rc != SQLITE_ROW) gagal(db, "baca cache_scan");

    ScanCache s{kolom_teks(cari.get(), 0), kolom_teks(cari.get(), 1),
                kolom_teks(cari.get(), 2), kolom_teks(cari.get(), 3),
                kolom_teks(cari.get(), 4)};

    rc = sqlite3_step(cari.get());
    if (rc == SQLITE_ROW)
        throw std::runtime_error("awalan hash ambigu, cocok lebih dari satu: " + awalan_hash);
    if (rc != SQLITE_DONE) gagal(db, "baca cache_scan");
    cari.reset();  // lepas statement SELECT sebelum DELETE jalan

    std::cout << "Akan dihapus:\n"
              << "  hash     : " << s.hash << "\n"
              << "  judul    : " << s.judul << "\n"
              << "  penulis  : " << s.penulis << "\n"
              << "  kategori : " << s.kategori << "\n"
              << "  isbn     : " << s.isbn << "\n";

    if (!yes) {
        std::cout << "Lanjut hapus? (y/N): ";
        std::string jawab;
        std::getline(std::cin, jawab);
        if (jawab != "y" && jawab != "Y") {
            std::cout << "Dibatalkan.\n";
            return;
        }
    }

    auto st_hapus = siapkan(db, "DELETE FROM cache_scan WHERE hash_gambar = ?");
    bind_teks(st_hapus.get(), 1, s.hash);
    if (sqlite3_step(st_hapus.get()) != SQLITE_DONE) gagal(db, "hapus cache_scan");
    std::cout << "[OK] " << sqlite3_changes(db) << " baris dihapus\n";
}
#pragma once

#include <string>
#include <vector>

#include "sqlite3.h"

// Hasil pencarian LOKAL: hanya tabel di file DB ini. Open Library (jaringan)
// sengaja tidak ada di C++, jadi langkah itu hanya dicatat di jejak.
struct HasilCari {
    bool ketemu = false;
    std::string judul, penulis;      // terisi di jalur ISBN
    std::string penerbit, isbn;      // kosong = tidak ada
    std::string sumber;              // asal data: <sumber>_cache, rag_manual, ...
    std::string status_cache;        // status entri cache_metadata, kalau ada
    std::vector<std::string> jejak;  // langkah yang dilalui, untuk ditampilkan
};

// Jalur scan cover (bagian lokal dari lengkapi_metadata di notebook):
// cache_metadata -> rag_manual.
HasilCari cari_metadata(sqlite3* db, const std::string& judul, const std::string& penulis);

// Jalur ISBN (bagian lokal dari cari_dari_isbn di notebook):
// cache_metadata + katalog -> rag_manual.
HasilCari cari_dari_isbn_lokal(sqlite3* db, const std::string& isbn);

void cetak_hasil(const HasilCari& h);
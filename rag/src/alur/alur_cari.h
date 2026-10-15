#pragma once

#include <string>
#include <vector>

#include "sqlite3.h"

// Hasil pencarian LOKAL (inti.db + cache.db). Open Library (jaringan) sengaja tidak ada di C++:
// langkah itu hanya dicatat di jejak, dan notebook yang menjalankannya.
// cache boleh nullptr (cache.db belum ada): langkah cache dilewati dan dicatat di jejak.
struct HasilCari {
    bool ketemu = false;
    bool tercatat_kosong = false;    // ISBN pernah dicari di luar dan tidak ketemu (cache_kosong)
    std::string judul, penulis;
    std::string penerbit, isbn;      // kosong = tidak ada
    std::string sumber;              // rag_manual, katalog (status), <sumber>_cache
    std::vector<std::string> jejak;  // langkah yang dilalui, untuk ditampilkan
};

// Urutan: rag_manual -> cache_isbn -> katalog -> cache_kosong. Inti didahulukan karena itu data
// yang sudah dibenarkan; kalau ISBN yang sama ada di dua tempat, inti yang menang.
HasilCari cari_isbn(sqlite3* inti, sqlite3* cache, const std::string& isbn_mentah);

// Urutan: rag_manual (persis) -> cache_isbn (persis) -> rag_manual (mirip, ambang 0.75) ->
// katalog (persis). Penulis kosong = cocok dengan penulis mana pun.
HasilCari cari_judul(sqlite3* inti, sqlite3* cache, const std::string& judul, const std::string& penulis);

// hash (atau awalannya) -> cache_scan -> ISBN -> cari_isbn; kalau belum ada ISBN, cari_judul
// dengan judul dan penulis hasil scan. Awalan yang cocok ke lebih dari satu hash = error.
HasilCari cari_hash(sqlite3* inti, sqlite3* cache, const std::string& awalan_hash);

void cetak_hasil(const HasilCari& h);
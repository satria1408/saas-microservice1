#pragma once

#include <string>

#include "sqlite3.h"

// cache_kosong = ISBN yang sudah dicari (Open Library) dan tidak ketemu, supaya tidak dicari
// berulang-ulang. Berumur pendek (lihat cache/cache_ttl.h).
void kosong_tambah(sqlite3* db, const std::string& isbn_mentah);
void kosong_daftar(sqlite3* db);
void kosong_hapus(sqlite3* db, const std::string& isbn_mentah);
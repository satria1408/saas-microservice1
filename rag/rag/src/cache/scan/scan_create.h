#pragma once

#include <string>

#include "sqlite3.h"

// isbn_mentah kosong = tidak ada barcode (pada upsert, ISBN lama dipertahankan).
void scan_tambah(sqlite3* db, const std::string& hash, const std::string& judul,
                 const std::string& penulis, const std::string& kategori,
                 const std::string& isbn_mentah);
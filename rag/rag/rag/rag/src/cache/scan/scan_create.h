#pragma once

#include <string>

#include "sqlite3.h"

void scan_tambah(sqlite3* db, const std::string& hash, const std::string& judul,
                 const std::string& penulis, const std::string& kategori);
#pragma once

#include <string>

#include "sqlite3.h"

void katalog_tambah(sqlite3* db, const std::string& judul, const std::string& penulis,
                    const std::string& penerbit, const std::string& isbn,
                    const std::string& stok);

void katalog_impor(sqlite3* db, const std::string& path, bool rinci);
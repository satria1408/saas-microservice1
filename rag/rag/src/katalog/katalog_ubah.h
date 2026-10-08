#pragma once

#include <string>

#include "sqlite3.h"

// penerbit/isbn/stok: kosong atau "-" berarti tidak diubah.
void katalog_konfirmasi(sqlite3* db, long long id, const std::string& penerbit,
                        const std::string& isbn, const std::string& stok, bool yes);
#pragma once

#include <string>

#include "sqlite3.h"

// Simpan atau perbarui satu entri cache_isbn. Nilai kosong tidak menimpa data lama, dan
// waktu_masuk diperbarui (data dianggap baru diambil). Entri cache_kosong untuk ISBN yang
// sama ikut dihapus karena ISBN-nya sekarang sudah ketemu.
void isbn_simpan(sqlite3* db, const std::string& isbn_mentah, const std::string& judul,
                 const std::string& penulis, const std::string& penerbit,
                 const std::string& sumber);
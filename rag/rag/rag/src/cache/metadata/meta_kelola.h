#pragma once

#include <string>
#include "db.h"

void meta_set_status(sqlite3* db, const std::string& key, const std::string& status);

// Hapus satu entri berdasarkan key (konfirmasi kecuali tanpa_tanya=true).
void meta_hapus(sqlite3* db, const std::string& key, bool tanpa_tanya);


void meta_bersihkan(sqlite3* db, int hari, bool tanpa_tanya);
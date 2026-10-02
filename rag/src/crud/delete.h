#pragma once
// DELETE: hapus satu baris berdasarkan ID (dengan konfirmasi kecuali -y).
#include "db.h"

void hapus(sqlite3* db, long long id, bool tanpa_tanya);

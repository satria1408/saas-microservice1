#pragma once
#include "opsi.h"
#include "sqlite3.h"

// rag migrasi                       -> siapkan/mutakhirkan skema inti.db dan cache.db
// rag migrasi pisah                 -> laporan: apa yang akan pindah dari DB lama (tidak menulis apa pun)
// rag migrasi pisah <inti> <cache>  -> buat dua file baru dan salin; DB lama tidak diubah
void perintah_migrasi(sqlite3* db, const Opsi& o);
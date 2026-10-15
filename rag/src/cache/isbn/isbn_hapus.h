#pragma once

#include <string>
#include <vector>

#include "sqlite3.h"

// cache hapus <isbn>: buang entri cache_isbn dan cache_kosong untuk ISBN itu (konfirmasi kecuali -y).
// cache_scan tidak disentuh: hash -> ISBN tetap benar walau bukunya sudah masuk inti.
void isbn_hapus(sqlite3* db, const std::string& isbn_mentah, bool yes);

// Dipanggil SETELAH buku berhasil masuk inti.db: entri cache untuk ISBN itu tidak berguna lagi.
// Tidak atomik dengan penulisan inti (dua file), jadi sengaja best-effort: cache null, belum
// ada, atau gagal -> hanya peringatan. Kalau terlewat, entri basi dibuang TTL.
void bersihkan_cache_setelah_inti(sqlite3* cache, const std::vector<std::string>& daftar_isbn);
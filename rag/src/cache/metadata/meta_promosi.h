#pragma once

#include <string>

#include "db.h"

void meta_promosi(sqlite3* db, const std::string& key,
                  const std::string& penerbit_baru, const std::string& isbn_baru);

// Promosi otomatis: menyisir cache_metadata (status baru/ditinjau) dan menaikkan yang LENGKAP
// ke rag_manual, tanpa pernah menimpa buku yang sudah ada. Yang masuk duluan (waktu_masuk
// paling awal) dipakai; yang datang belakangan dengan judul+penulis atau ISBN sama dilewati
// dan dibiarkan sampai TTL membuangnya.
// cek=true: hanya melaporkan, tidak menulis apa pun.
void meta_promosi_otomatis(sqlite3* db, bool cek, bool rinci);
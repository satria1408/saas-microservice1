#pragma once

#include "db.h"

// Dua file database yang dibuka untuk satu perintah. Yang tidak dibutuhkan perintah itu (atau
// cache.db yang memang belum ada) dibiarkan null.
struct Koneksi {
    DbPtr inti;   // inti.db  : rag_manual + katalog
    DbPtr cache;  // cache.db : cache_scan + cache_isbn + cache_kosong
};
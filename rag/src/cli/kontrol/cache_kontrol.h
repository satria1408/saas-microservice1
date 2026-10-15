#pragma once
#include "opsi.h"
#include "sqlite3.h"

// rag cache list | get | add | hapus | kosong | bersihkan | scan ...
// db = koneksi ke cache.db.
void perintah_cache(sqlite3* db, const Opsi& o);
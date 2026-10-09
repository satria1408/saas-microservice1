#pragma once
#include "opsi.h"
#include "sqlite3.h"

// rag cache scan list | get | add | hapus | bersihkan
// Dipanggil dari perintah_cache. args[0]="cache", args[1]="scan", args[2]=aksi.
void perintah_cache_scan(sqlite3* db, const Opsi& o);
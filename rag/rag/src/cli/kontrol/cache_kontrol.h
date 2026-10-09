#pragma once
#include "opsi.h"
#include "sqlite3.h"

// rag cache list | add | status | hapus | bersihkan | ttl | promosi | scan ...
void perintah_cache(sqlite3* db, const Opsi& o);
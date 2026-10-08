#pragma once
#include "opsi.h"
#include "sqlite3.h"

// rag migrasi: backup dulu (kecuali --no-backup), lalu mutakhirkan skema
// rag_manual, cache_metadata, cache_scan, dan katalog.
void perintah_migrasi(sqlite3* db, const Opsi& o);
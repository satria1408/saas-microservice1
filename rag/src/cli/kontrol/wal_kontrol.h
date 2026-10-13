#pragma once
#include "opsi.h"
#include "sqlite3.h"

// rag wal                  -> status (baca saja)
// rag wal pasang [--paksa] -> backup, lalu pindah ke mode WAL
// rag wal lepas            -> backup, lalu kembali ke mode DELETE
// Perintah ini mengurus backup sendiri (seperti migrasi), jadi middleware melewatinya.
void perintah_wal(sqlite3* db, const Opsi& o);
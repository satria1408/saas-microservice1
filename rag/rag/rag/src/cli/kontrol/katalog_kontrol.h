#pragma once
#include "opsi.h"
#include "sqlite3.h"

// rag katalog list | get | konfirmasi
void perintah_katalog(sqlite3* db, const Opsi& o);
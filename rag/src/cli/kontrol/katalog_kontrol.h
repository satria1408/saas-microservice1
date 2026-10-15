#pragma once
#include "opsi.h"
#include "sqlite3.h"

// rag katalog list | get | tambah | impor | konfirmasi (tabel katalog di inti.db)
// cache (boleh nullptr) hanya dipakai untuk membuang entri cache setelah konfirmasi.
void perintah_katalog(sqlite3* inti, sqlite3* cache, const Opsi& o);
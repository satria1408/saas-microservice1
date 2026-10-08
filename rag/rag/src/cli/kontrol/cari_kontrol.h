#pragma once
#include "opsi.h"
#include "sqlite3.h"

// rag cari <judul> [penulis]
void perintah_cari(sqlite3* db, const Opsi& o);

// rag cari-isbn <isbn>
void perintah_cari_isbn(sqlite3* db, const Opsi& o);
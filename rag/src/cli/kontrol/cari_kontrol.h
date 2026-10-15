// cari_kontrol.h
#pragma once
#include "opsi.h"
#include "sqlite3.h"

// cache boleh nullptr (cache.db belum ada). Ketiganya hanya membaca.
// rag cari <judul> [penulis]
void perintah_cari(sqlite3* inti, sqlite3* cache, const Opsi& o);

// rag cari-isbn <isbn>
void perintah_cari_isbn(sqlite3* inti, sqlite3* cache, const Opsi& o);

// rag cari-hash <hash atau awalannya>
void perintah_cari_hash(sqlite3* inti, sqlite3* cache, const Opsi& o);
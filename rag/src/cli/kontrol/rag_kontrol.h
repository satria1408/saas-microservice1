// rag_kontrol.h
#pragma once
#include "opsi.h"
#include "sqlite3.h"

// add, import, list, edit, hapus (tabel rag_manual di inti.db).
// Perintah di o.args[0] sudah dipastikan salah satu dari kelimanya oleh router.
// cache (boleh nullptr) hanya dipakai untuk membuang entri cache setelah buku masuk inti.
void perintah_rag(sqlite3* inti, sqlite3* cache, const Opsi& o);
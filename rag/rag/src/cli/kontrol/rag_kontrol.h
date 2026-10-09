#pragma once
#include "opsi.h"
#include "sqlite3.h"

// add, import, list, edit, hapus (tabel rag_manual).
// Perintah di o.args[0] sudah dipastikan salah satu dari kelimanya oleh router.
void perintah_rag(sqlite3* db, const Opsi& o);
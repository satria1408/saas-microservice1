#pragma once

#include <string>

#include "db.h"

// Pasang aturan TTL. Satuan hari, 0 = status itu tidak pernah dibuang otomatis.
// 'ditinjau' tidak pernah dibuang. Aturan dijalankan trigger setiap ada INSERT
// ke cache_metadata, dari penulis mana pun (rag.exe maupun notebook Python).
void meta_pasang_ttl(sqlite3* db, int hari_ditolak, int hari_dipromosikan, int hari_baru);

// Cabut trigger TTL (trigger pengisi waktu_masuk dibiarkan, tidak berbahaya).
void meta_lepas_ttl(sqlite3* db);
void meta_status_ttl(sqlite3* db);
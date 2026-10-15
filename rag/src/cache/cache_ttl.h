#pragma once

#include "sqlite3.h"

// cache bersihkan [--hari N] [-y]: buang baris lama dari cache_isbn, cache_kosong, dan cache_scan
// dalam satu transaksi. hari = 0 memakai umur bawaan per tabel (isbn 90, kosong 7, scan 90);
// hari > 0 dipakai untuk ketiganya. Baris tanpa waktu_masuk tidak pernah dibuang.
// Tanpa trigger dan tanpa tabel aturan: cache boleh dibuang kapan saja.
void cache_bersihkan(sqlite3* db, int hari, bool yes);
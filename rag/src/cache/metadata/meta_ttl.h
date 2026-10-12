#pragma once

#include <functional>
#include <string>

#include "db.h"

struct AturanTtl {
    int meta_ditolak = 7;
    int meta_dipromosikan = 0;
    int meta_baru = 90;
    int scan = 90;
    int katalog_otomatis = 90;
};

void ttl_pasang(sqlite3* db, const AturanTtl& aturan);

void ttl_lepas(sqlite3* db);

void ttl_status(sqlite3* db);

void ttl_bersihkan(sqlite3* db, int hari_override, bool yes,
                   const std::function<void()>& sebelum_tulis = {});
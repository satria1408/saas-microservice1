#pragma once

#include <string>

#include "sqlite3.h"

void scan_hapus(sqlite3* db, const std::string& awalan_hash, bool yes);
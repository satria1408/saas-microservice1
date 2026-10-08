#pragma once

#include <string>

#include "sqlite3.h"

void scan_daftar(sqlite3* db, const std::string& kata);
void scan_tampil(sqlite3* db, const std::string& awalan_hash);
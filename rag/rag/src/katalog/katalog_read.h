#pragma once

#include <string>

#include "sqlite3.h"

bool status_katalog_valid(const std::string& s);
void katalog_daftar(sqlite3* db, const std::string& status, const std::string& kata);
void katalog_tampil(sqlite3* db, long long id);
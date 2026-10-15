#pragma once

#include <string>

#include "sqlite3.h"

void isbn_daftar(sqlite3* db, const std::string& kata);
void isbn_tampil(sqlite3* db, const std::string& isbn_mentah);
#pragma once
// READ: tampilkan isi rag_manual, opsional difilter satu kata kunci.
#include <string>

#include "db.h"

void daftar(sqlite3* db, const std::string& kata);

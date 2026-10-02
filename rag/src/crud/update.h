#pragma once
#include <string>

#include "db.h"

// field: judul | penulis | penerbit | isbn
void edit(sqlite3* db, long long id, const std::string& field, const std::string& nilai);

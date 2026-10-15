#pragma once
#include <string>
#include <vector>

#include "db.h"

// field: judul | penulis | penerbit | isbn
void edit(sqlite3* db, long long id, const std::string& field, const std::string& nilai,
          std::vector<std::string>* isbn_masuk = nullptr);
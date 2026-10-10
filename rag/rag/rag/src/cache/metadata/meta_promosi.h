#pragma once

#include <string>

#include "db.h"

void meta_promosi(sqlite3* db, const std::string& key,
                  const std::string& penerbit_baru, const std::string& isbn_baru);
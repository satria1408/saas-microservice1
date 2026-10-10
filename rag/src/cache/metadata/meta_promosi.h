#pragma once

#include <functional>
#include <string>

#include "db.h"

void meta_promosi(sqlite3* db, const std::string& key,
                  const std::string& penerbit_baru, const std::string& isbn_baru);

void meta_promosi_otomatis(sqlite3* db, bool cek, bool rinci,
                           const std::function<void()>& sebelum_tulis = {});
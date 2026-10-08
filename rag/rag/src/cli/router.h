#pragma once
#include "opsi.h"
#include "sqlite3.h"

// Arahkan perintah ke modul kontrol yang sesuai. Melempar std::runtime_error
// kalau perintahnya tidak dikenal.
void arahkan(sqlite3* db, const Opsi& o);
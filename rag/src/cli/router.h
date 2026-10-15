#pragma once
#include "koneksi.h"
#include "opsi.h"

// Arahkan perintah ke modul kontrol yang sesuai. Melempar std::runtime_error
// kalau perintahnya tidak dikenal.
void arahkan(Koneksi& k, const Opsi& o);
#pragma once
#include <string>
#include <vector>

struct Opsi {
    std::string db;
    bool no_backup = false, tanpa_transaksi = false, rinci = false, yes = false;
    int hari = 0;
    int simpan_backup = 5;
    std::vector<std::string> args;

    // Argumen posisi ke-i, string kosong kalau tidak ada.
    std::string arg(size_t i) const { return i < args.size() ? args[i] : std::string(); }
};

Opsi baca_opsi(int argc, char** argv);

// Pembantu baca argumen. Semuanya melempar std::runtime_error kalau tidak valid.
int baca_angka(const std::string& s, const char* nama);
long long baca_id(const std::string& s, const char* perintah);
const std::string& wajib(const std::string& nilai, const char* pesan);
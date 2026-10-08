#include "opsi.h"

#include <cctype>
#include <cstdlib>
#include <stdexcept>

int baca_angka(const std::string& s, const char* nama) {
    if (s.empty() || s.size() > 6 || s.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error(std::string(nama) + " butuh angka (maksimal 6 digit)");
    return std::stoi(s);
}

long long baca_id(const std::string& s, const char* perintah) {
    if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error(std::string(perintah) + " butuh <id> berupa angka");
    return std::stoll(s);
}

const std::string& wajib(const std::string& nilai, const char* pesan) {
    if (nilai.empty()) throw std::runtime_error(pesan);
    return nilai;
}

Opsi baca_opsi(int argc, char** argv) {
    Opsi o;
    if (const char* e = std::getenv("RAG_DB")) o.db = e;
    else o.db = "book_cache.db";
    for (int k = 1; k < argc; ++k) {
        std::string a = argv[k];
        if (a == "--db") {
            if (k + 1 >= argc) throw std::runtime_error("--db butuh path");
            o.db = argv[++k];
        } else if (a == "--hari") {
            if (k + 1 >= argc) throw std::runtime_error("--hari butuh angka");
            o.hari = baca_angka(argv[++k], "--hari");
        } else if (a == "--simpan-backup") {
            if (k + 1 >= argc) throw std::runtime_error("--simpan-backup butuh angka");
            o.simpan_backup = baca_angka(argv[++k], "--simpan-backup");
            if (o.simpan_backup < 1) throw std::runtime_error("--simpan-backup minimal 1");
        } else if (a == "--no-backup") o.no_backup = true;
        else if (a == "--tanpa-transaksi") o.tanpa_transaksi = true;
        else if (a == "--rinci") o.rinci = true;
        else if (a == "-y") o.yes = true;
        else if (a.size() > 1 && a[0] == '-' && !std::isdigit(static_cast<unsigned char>(a[1])))
            throw std::runtime_error("opsi tidak dikenal: " + a);
        else o.args.push_back(a);
    }
    return o;
}
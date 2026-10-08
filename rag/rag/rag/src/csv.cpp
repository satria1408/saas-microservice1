#include "csv.h"

#include <fstream>
#include <iterator>
#include <stdexcept>

std::vector<std::vector<std::string>> baca_csv(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Tidak bisa membuka CSV: " + path);
    std::string isi((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (isi.rfind("\xEF\xBB\xBF", 0) == 0) isi.erase(0, 3); 

    std::vector<std::vector<std::string>> baris;
    std::vector<std::string> rec;
    std::string sel;
    bool dalam_kutip = false, ada_isi = false;

    for (size_t i = 0; i < isi.size(); ++i) {
        char c = isi[i];
        if (dalam_kutip) {
            if (c == '"') {
                if (i + 1 < isi.size() && isi[i + 1] == '"') { sel += '"'; ++i; }
                else dalam_kutip = false;
            } else {
                sel += c;
            }
        } else if (c == '"') {
            dalam_kutip = true;
            ada_isi = true;
        } else if (c == ',') {
            rec.push_back(sel);
            sel.clear();
            ada_isi = true;
        } else if (c == '\r') {
            // abaikan (CRLF Windows)
        } else if (c == '\n') {
            if (ada_isi || !sel.empty()) { rec.push_back(sel); baris.push_back(rec); }
            rec.clear();
            sel.clear();
            ada_isi = false;
        } else {
            sel += c;
            ada_isi = true;
        }
    }
    if (dalam_kutip) throw std::runtime_error("CSV: ada tanda kutip yang tidak ditutup");
    if (ada_isi || !sel.empty()) { rec.push_back(sel); baris.push_back(rec); }
    return baris;
}

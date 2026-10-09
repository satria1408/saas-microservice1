#include "util.h"

#include <cctype>

bool ean13_valid(std::string_view s) {
    if (s.size() != 13) return false;
    int total = 0;
    for (size_t i = 0; i < 13; ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
        int d = s[i] - '0';
        if (i < 12) total += d * (i % 2 == 0 ? 1 : 3);
    }
    return (10 - total % 10) % 10 == (s[12] - '0');
}

std::optional<std::string> bersihkan_isbn(const std::string& mentah) {
    std::string out;
    for (char c : mentah)
        if (c != '-' && c != ' ') out += c;
    if (out.empty() || out == "null" || out == "None") return std::nullopt;
    return out;
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string huruf_kecil(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

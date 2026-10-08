#pragma once
#include <optional>
#include <string>
#include <string_view>

bool ean13_valid(std::string_view s);
std::optional<std::string> bersihkan_isbn(const std::string& mentah);
std::string trim(const std::string& s);
std::string huruf_kecil(std::string s);

inline std::string batas_atas_awalan(std::string awalan) {
    while (!awalan.empty()) {
        unsigned char c = static_cast<unsigned char>(awalan.back());
        if (c < 0xFF) { awalan.back() = static_cast<char>(c + 1); return awalan; }
        awalan.pop_back();
    }
    return awalan;
}
#pragma once
#include <optional>
#include <string>
#include <string_view>

bool ean13_valid(std::string_view s);
std::optional<std::string> bersihkan_isbn(const std::string& mentah);
std::string trim(const std::string& s);
std::string huruf_kecil(std::string s);

// ISBN-13 = EAN-13 dengan checksum benar DAN awalan 978 atau 979. Barcode produk lain
// (misalnya awalan 899) bisa lolos ean13_valid tapi bukan ISBN.
inline bool isbn13_valid(std::string_view s) {
    return ean13_valid(s) && (s.substr(0, 3) == "978" || s.substr(0, 3) == "979");
}

inline std::string batas_atas_awalan(std::string awalan) {
    while (!awalan.empty()) {
        unsigned char c = static_cast<unsigned char>(awalan.back());
        if (c < 0xFF) { awalan.back() = static_cast<char>(c + 1); return awalan; }
        awalan.pop_back();
    }
    return awalan;
}
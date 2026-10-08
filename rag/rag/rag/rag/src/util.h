#pragma once
#include <optional>
#include <string>
#include <string_view>

bool ean13_valid(std::string_view s);
std::optional<std::string> bersihkan_isbn(const std::string& mentah);
std::string trim(const std::string& s);
std::string huruf_kecil(std::string s);

#pragma once
// Parser CSV: kutip, koma di dalam kutip, "" sebagai kutip literal, CRLF, BOM UTF-8.
#include <string>
#include <vector>

std::vector<std::vector<std::string>> baca_csv(const std::string& path);

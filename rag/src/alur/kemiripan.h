#pragma once

#include <string>

// Dekode UTF-8 ke daftar karakter, lalu jadikan huruf kecil.
std::u32string normalisasi_kecil(const std::string& utf8);

// a dan b harus sudah lewat normalisasi_kecil (notebook melakukan .lower() sebelum membandingkan).
double rasio_difflib(const std::u32string& a, const std::u32string& b);
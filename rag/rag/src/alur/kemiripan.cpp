#include "alur/kemiripan.h"

#include <unordered_map>
#include <utility>
#include <vector>

namespace {

char32_t huruf_kecil(char32_t c) {
    if (c >= U'A' && c <= U'Z') return c + 32;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 32;  // Latin-1: À..Þ kecuali ×
    return c;
}

struct Blok { int i, j, k; };

class Pencocok {
public:
    Pencocok(const std::u32string& a, const std::u32string& b) : a_(a), b_(b) {
        const int n = static_cast<int>(b_.size());
        for (int i = 0; i < n; ++i) b2j_[b_[i]].push_back(i);
        
        if (n >= 200) {
            const int batas = n / 100 + 1;
            for (auto it = b2j_.begin(); it != b2j_.end();) {
                if (static_cast<int>(it->second.size()) > batas) it = b2j_.erase(it);
                else ++it;
            }
        }
    }

    int jumlah_cocok() const {
        struct Wilayah { int alo, ahi, blo, bhi; };
        std::vector<Wilayah> antrean;
        antrean.push_back({0, static_cast<int>(a_.size()), 0, static_cast<int>(b_.size())});
        int total = 0;
        while (!antrean.empty()) {
            const Wilayah w = antrean.back();
            antrean.pop_back();
            const Blok x = terpanjang(w.alo, w.ahi, w.blo, w.bhi);
            if (x.k) {
                total += x.k;
                if (w.alo < x.i && w.blo < x.j)
                    antrean.push_back({w.alo, x.i, w.blo, x.j});
                if (x.i + x.k < w.ahi && x.j + x.k < w.bhi)
                    antrean.push_back({x.i + x.k, w.ahi, x.j + x.k, w.bhi});
            }
        }
        return total;
    }

private:
    Blok terpanjang(int alo, int ahi, int blo, int bhi) const {
        int besti = alo, bestj = blo, bestsize = 0;
        std::unordered_map<int, int> j2len;
        for (int i = alo; i < ahi; ++i) {
            std::unordered_map<int, int> baru;
            const auto it = b2j_.find(a_[i]);
            if (it != b2j_.end()) {
                for (int j : it->second) {
                    if (j < blo) continue;
                    if (j >= bhi) break;
                    const auto p = j2len.find(j - 1);
                    const int k = (p == j2len.end() ? 0 : p->second) + 1;
                    baru[j] = k;
                    if (k > bestsize) {
                        besti = i - k + 1;
                        bestj = j - k + 1;
                        bestsize = k;
                    }
                }
            }
            j2len = std::move(baru);
        }
        // Perpanjang ke dua arah: karakter "populer" yang dibuang dari indeks tetap
        // boleh ikut kalau menempel pada blok yang ditemukan.
        while (besti > alo && bestj > blo && a_[besti - 1] == b_[bestj - 1]) {
            --besti; --bestj; ++bestsize;
        }
        while (besti + bestsize < ahi && bestj + bestsize < bhi &&
               a_[besti + bestsize] == b_[bestj + bestsize]) {
            ++bestsize;
        }
        return {besti, bestj, bestsize};
    }

    const std::u32string& a_;
    const std::u32string& b_;
    std::unordered_map<char32_t, std::vector<int>> b2j_;
};

}  

std::u32string normalisasi_kecil(const std::string& s) {
    std::u32string hasil;
    size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        char32_t cp = c;
        size_t n = 1;
        if (c >= 0xC0 && c < 0xE0) { cp = c & 0x1F; n = 2; }
        else if (c >= 0xE0 && c < 0xF0) { cp = c & 0x0F; n = 3; }
        else if (c >= 0xF0 && c < 0xF8) { cp = c & 0x07; n = 4; }
        bool ok = (n == 1 || i + n <= s.size());
        for (size_t k = 1; ok && k < n; ++k) {
            const unsigned char cc = static_cast<unsigned char>(s[i + k]);
            if ((cc & 0xC0) != 0x80) ok = false;
            else cp = (cp << 6) | (cc & 0x3F);
        }
        if (!ok) { cp = c; n = 1; }  // byte tidak valid: perlakukan sebagai satu karakter
        hasil.push_back(huruf_kecil(cp));
        i += n;
    }
    return hasil;
}

double rasio_difflib(const std::u32string& a, const std::u32string& b) {
    const size_t panjang = a.size() + b.size();
    if (panjang == 0) return 1.0;
    const Pencocok p(a, b);
    return 2.0 * p.jumlah_cocok() / static_cast<double>(panjang);
}
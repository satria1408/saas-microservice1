#include "alur/alur_cari.h"

#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>

#include "alur/kemiripan.h"
#include "db.h"
#include "util.h"

namespace {

// Seperti kolom_teks, tapi NULL tetap NULL (kolom_teks mengubahnya jadi "-").
std::optional<std::string> kolom_opsional(sqlite3_stmt* st, int i) {
    const unsigned char* p = sqlite3_column_text(st, i);
    if (!p) return std::nullopt;
    return std::string(reinterpret_cast<const char*>(p));
}

std::string atau_kosong(const std::optional<std::string>& s) { return s ? *s : std::string(); }

using Baris = std::vector<std::string>;  // kolom hasil, NULL menjadi ""

// Satu baris pertama dari SELECT dengan parameter teks ?1, ?2, ...; nullopt kalau kosong.
std::optional<Baris> satu_baris(sqlite3* db, const char* sql, const std::vector<std::string>& param,
                                int jumlah_kolom, const char* konteks) {
    auto st = siapkan(db, sql);
    for (size_t i = 0; i < param.size(); ++i) bind_teks(st.get(), static_cast<int>(i) + 1, param[i]);
    const int rc = sqlite3_step(st.get());
    if (rc == SQLITE_DONE) return std::nullopt;
    if (rc != SQLITE_ROW) gagal(db, konteks);
    Baris b;
    for (int i = 0; i < jumlah_kolom; ++i) b.push_back(atau_kosong(kolom_opsional(st.get(), i)));
    return b;
}

// Sama dengan ambang_batas di cari_di_rag_manual (notebook).
constexpr double kAmbangMirip = 0.75;

// Cocok judul persis (huruf besar-kecil diabaikan); penulis kosong = penulis mana pun.
const char* kCocokJudul = "LOWER(judul) = LOWER(?1) AND (?2 = '' OR LOWER(COALESCE(penulis,'')) = LOWER(?2))";

void isi(HasilCari& h, const std::string& judul, const std::string& penulis,
         const std::string& penerbit, const std::string& isbn, const std::string& sumber) {
    h.ketemu = true;
    h.judul = judul;
    h.penulis = penulis;
    h.penerbit = penerbit;
    if (!isbn.empty()) h.isbn = isbn;
    h.sumber = sumber;
}

// Hasil dari rag_manual/cache yang ISBN-nya kosong: coba ambil dari katalog (judul+penulis persis).
void lengkapi_isbn(sqlite3* inti, HasilCari& h, const std::string& judul, const std::string& penulis) {
    if (!h.ketemu || !h.isbn.empty()) return;
    const std::string sql = std::string("SELECT isbn FROM katalog WHERE ") + kCocokJudul +
                            " AND isbn IS NOT NULL AND isbn <> '' ORDER BY id LIMIT 1";
    if (auto r = satu_baris(inti, sql.c_str(), {judul, penulis}, 1, "baca katalog")) {
        h.isbn = (*r)[0];
        h.jejak.push_back("isbn kosong, diambil dari katalog");
    }
}

HasilCari gabung(const HasilCari& awal, HasilCari lanjut) {
    std::vector<std::string> j = awal.jejak;
    j.insert(j.end(), lanjut.jejak.begin(), lanjut.jejak.end());
    lanjut.jejak = std::move(j);
    return lanjut;
}

}  // namespace

HasilCari cari_isbn(sqlite3* inti, sqlite3* cache, const std::string& isbn_mentah) {
    HasilCari h;
    const auto bersih = bersihkan_isbn(isbn_mentah);
    if (!bersih) {
        h.jejak.push_back("isbn kosong: tidak ada yang dicari");
        return h;
    }
    const std::string isbn = *bersih;
    h.isbn = isbn;
    h.jejak.push_back("isbn bersih: " + isbn);
    if (!isbn13_valid(isbn)) {
        h.jejak.push_back("bukan ISBN-13 yang valid (978/979 + checksum), tidak dicari");
        return h;
    }

    if (auto r = satu_baris(inti, "SELECT judul, penulis, penerbit FROM rag_manual WHERE isbn = ?1",
                            {isbn}, 3, "baca rag_manual")) {
        isi(h, (*r)[0], (*r)[1], (*r)[2], isbn, "rag_manual");
        h.jejak.push_back("rag_manual: ketemu (ISBN persis)");
        return h;
    }
    h.jejak.push_back("rag_manual: tidak ada");

    if (cache) {
        if (auto r = satu_baris(cache,
                "SELECT judul, penulis, penerbit, sumber FROM cache_isbn WHERE isbn = ?1",
                {isbn}, 4, "baca cache_isbn")) {
            isi(h, (*r)[0], (*r)[1], (*r)[2], isbn, (*r)[3].empty() ? "cache_isbn" : (*r)[3] + "_cache");
            h.jejak.push_back("cache_isbn: ketemu");
            return h;
        }
        h.jejak.push_back("cache_isbn: tidak ada");
    } else {
        h.jejak.push_back("cache.db: tidak ada, cache_isbn dan cache_kosong dilewati");
    }

    if (auto r = satu_baris(inti,
            "SELECT judul, penulis, penerbit, status_konfirmasi FROM katalog WHERE isbn = ?1 ORDER BY id LIMIT 1",
            {isbn}, 4, "baca katalog")) {
        isi(h, (*r)[0], (*r)[1], (*r)[2], isbn, "katalog (" + ((*r)[3].empty() ? std::string("-") : (*r)[3]) + ")");
        h.jejak.push_back("katalog: ketemu (staging, belum tentu lengkap)");
        return h;
    }
    h.jejak.push_back("katalog: tidak ada");

    if (cache) {
        if (auto r = satu_baris(cache, "SELECT waktu_masuk FROM cache_kosong WHERE isbn = ?1", {isbn}, 1, "baca cache_kosong")) {
            h.tercatat_kosong = true;
            h.jejak.push_back("cache_kosong: pernah dicari di luar dan tidak ketemu (" + (*r)[0] + ")");
            return h;
        }
        h.jejak.push_back("cache_kosong: belum pernah dicari");
    }
    h.jejak.push_back("langkah berikutnya di notebook: Open Library (tidak dijalankan di C++)");
    return h;
}

HasilCari cari_judul(sqlite3* inti, sqlite3* cache, const std::string& judul_mentah,
                     const std::string& penulis_mentah) {
    HasilCari h;
    const std::string judul = trim(judul_mentah), penulis = trim(penulis_mentah);
    if (judul.empty()) {
        h.jejak.push_back("judul kosong: tidak ada yang dicari");
        return h;
    }

    const std::string sql_rag = std::string("SELECT judul, penulis, penerbit, isbn FROM rag_manual WHERE ") +
                                kCocokJudul + " ORDER BY id LIMIT 1";
    if (auto r = satu_baris(inti, sql_rag.c_str(), {judul, penulis}, 4, "baca rag_manual")) {
        isi(h, (*r)[0], (*r)[1], (*r)[2], (*r)[3], "rag_manual");
        h.jejak.push_back("rag_manual: ketemu (judul persis)");
        lengkapi_isbn(inti, h, judul, penulis);
        return h;
    }
    h.jejak.push_back("rag_manual: tidak ada yang persis");

    if (cache) {
        const std::string sql_cache = std::string("SELECT judul, penulis, penerbit, isbn, sumber FROM cache_isbn WHERE ") +
                                      kCocokJudul + " ORDER BY waktu_masuk DESC LIMIT 1";
        if (auto r = satu_baris(cache, sql_cache.c_str(), {judul, penulis}, 5, "baca cache_isbn")) {
            isi(h, (*r)[0], (*r)[1], (*r)[2], (*r)[3], (*r)[4].empty() ? "cache_isbn" : (*r)[4] + "_cache");
            h.jejak.push_back("cache_isbn: ketemu (judul persis)");
            return h;
        }
        h.jejak.push_back("cache_isbn: tidak ada yang persis");
    } else {
        h.jejak.push_back("cache.db: tidak ada, cache_isbn dilewati");
    }

    // rag_manual, mirip: skor difflib atas "judul penulis" (huruf kecil), ambil yang tertinggi,
    // minimal 0.75 (sama dengan cari_di_rag_manual di notebook).
    {
        const std::u32string query = normalisasi_kecil(judul + " " + penulis);
        auto st = siapkan(inti, "SELECT judul, penulis, penerbit, isbn FROM rag_manual");
        double skor_terbaik = 0.0;
        bool ada_kandidat = false;
        Baris terbaik;
        int rc;
        while ((rc = sqlite3_step(st.get())) == SQLITE_ROW) {
            Baris b;
            for (int i = 0; i < 4; ++i) b.push_back(atau_kosong(kolom_opsional(st.get(), i)));
            const double skor = rasio_difflib(query, normalisasi_kecil(b[0] + " " + b[1]));
            if (skor > skor_terbaik) {  // strict: kalau seri, yang lebih dulu menang (seperti notebook)
                skor_terbaik = skor;
                ada_kandidat = true;
                terbaik = b;
            }
        }
        if (rc != SQLITE_DONE) gagal(inti, "baca rag_manual");

        std::ostringstream skor_teks;
        skor_teks << std::fixed << std::setprecision(3) << skor_terbaik;
        if (ada_kandidat && skor_terbaik >= kAmbangMirip) {
            isi(h, terbaik[0], terbaik[1], terbaik[2], terbaik[3], "rag_manual (mirip)");
            h.jejak.push_back("rag_manual: ketemu mirip (skor " + skor_teks.str() + ")");
            lengkapi_isbn(inti, h, terbaik[0], terbaik[1]);
            return h;
        }
        h.jejak.push_back(ada_kandidat
            ? "rag_manual: tidak ada yang cukup mirip (skor tertinggi " + skor_teks.str() + " < 0.75): " +
              terbaik[0] + " | " + terbaik[1]
            : "rag_manual: kosong");
    }

    const std::string sql_kat = std::string("SELECT judul, penulis, penerbit, isbn, status_konfirmasi FROM katalog WHERE ") +
                                kCocokJudul + " ORDER BY id LIMIT 1";
    if (auto r = satu_baris(inti, sql_kat.c_str(), {judul, penulis}, 5, "baca katalog")) {
        isi(h, (*r)[0], (*r)[1], (*r)[2], (*r)[3], "katalog (" + ((*r)[4].empty() ? std::string("-") : (*r)[4]) + ")");
        h.jejak.push_back("katalog: ketemu (staging, belum tentu lengkap)");
        return h;
    }
    h.jejak.push_back("katalog: tidak ada yang persis");
    h.jejak.push_back("langkah berikutnya di notebook: Open Library (tidak dijalankan di C++)");
    return h;
}

HasilCari cari_hash(sqlite3* inti, sqlite3* cache, const std::string& awalan_mentah) {
    HasilCari h;
    const std::string awalan = trim(awalan_mentah);
    if (awalan.empty()) {
        h.jejak.push_back("hash kosong: tidak ada yang dicari");
        return h;
    }
    if (!cache) {
        h.jejak.push_back("cache.db tidak ada: cache_scan tidak bisa dibuka");
        return h;
    }

    std::string hash, judul, penulis, kategori, isbn;
    {
        auto st = siapkan(cache,
            "SELECT hash_gambar, judul, penulis, kategori, isbn FROM cache_scan "
            "WHERE hash_gambar >= ?1 AND hash_gambar < ?2 LIMIT 2");
        bind_teks(st.get(), 1, awalan);
        bind_teks(st.get(), 2, batas_atas_awalan(awalan));
        int rc = sqlite3_step(st.get());
        if (rc == SQLITE_DONE) {
            h.jejak.push_back("cache_scan: hash belum pernah discan");
            h.jejak.push_back("langkah berikutnya di notebook: baca cover lalu simpan (cache scan add)");
            return h;
        }
        if (rc != SQLITE_ROW) gagal(cache, "baca cache_scan");
        hash = atau_kosong(kolom_opsional(st.get(), 0));
        judul = atau_kosong(kolom_opsional(st.get(), 1));
        penulis = atau_kosong(kolom_opsional(st.get(), 2));
        kategori = atau_kosong(kolom_opsional(st.get(), 3));
        isbn = atau_kosong(kolom_opsional(st.get(), 4));
        rc = sqlite3_step(st.get());
        if (rc == SQLITE_ROW) throw std::runtime_error("awalan hash ambigu, cocok lebih dari satu: " + awalan);
        if (rc != SQLITE_DONE) gagal(cache, "baca cache_scan");
    }

    std::string pendek = hash.size() > 12 ? hash.substr(0, 12) : hash;
    h.jejak.push_back("cache_scan: ketemu " + pendek + " (judul: " + (judul.empty() ? "-" : judul) +
                      ", penulis: " + (penulis.empty() ? "-" : penulis) +
                      ", kategori: " + (kategori.empty() ? "-" : kategori) + ")");
    if (!trim(isbn).empty()) {
        h.jejak.push_back("cache_scan: sudah punya ISBN, dicari lewat ISBN");
        return gabung(h, cari_isbn(inti, cache, isbn));
    }
    if (!judul.empty()) {
        h.jejak.push_back("cache_scan: belum punya ISBN, dicari lewat judul+penulis");
        return gabung(h, cari_judul(inti, cache, judul, penulis));
    }
    h.jejak.push_back("cache_scan: tidak punya ISBN maupun judul, tidak ada yang bisa dicari");
    return h;
}

void cetak_hasil(const HasilCari& h) {
    const auto tampil = [](const std::string& s) { return s.empty() ? std::string("-") : s; };

    std::cout << "Jejak:\n";
    for (const auto& j : h.jejak) std::cout << "  - " << j << "\n";

    if (!h.ketemu) {
        std::cout << (h.tercatat_kosong ? "Hasil: tidak ketemu (sudah pernah dicari di luar, tercatat kosong)\n"
                                        : "Hasil: tidak ketemu di DB lokal\n");
        return;
    }
    std::cout << "Hasil:\n"
              << "  judul    : " << tampil(h.judul) << "\n"
              << "  penulis  : " << tampil(h.penulis) << "\n"
              << "  penerbit : " << tampil(h.penerbit) << "\n"
              << "  isbn     : " << tampil(h.isbn) << "\n"
              << "  sumber   : " << h.sumber << "\n";
}
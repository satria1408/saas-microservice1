#include "alur/alur_cari.h"

#include <cctype>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>

#include "alur/kemiripan.h"
#include "cache/metadata/meta_create.h"  // buat_key
#include "db.h"

namespace {

// Seperti kolom_teks, tapi NULL tetap NULL (kolom_teks mengubahnya jadi "-").
std::optional<std::string> kolom_opsional(sqlite3_stmt* st, int i) {
    const unsigned char* p = sqlite3_column_text(st, i);
    if (!p) return std::nullopt;
    return std::string(reinterpret_cast<const char*>(p));
}

std::string atau_kosong(const std::optional<std::string>& s) {
    return s ? *s : std::string();
}

bool terisi(const std::optional<std::string>& s) { return s && !s->empty(); }

// Sama dengan ambang_batas di cari_di_rag_manual (notebook).
constexpr double kAmbangMirip = 0.75;

// Sama dengan cari_dari_isbn di notebook: buang spasi dan tanda hubung.
std::string bersihkan_isbn(const std::string& mentah) {
    std::string hasil;
    for (char c : mentah) {
        if (c == '-' || std::isspace(static_cast<unsigned char>(c))) continue;
        hasil += c;
    }
    return hasil;
}

}  // namespace

HasilCari cari_metadata(sqlite3* db, const std::string& judul, const std::string& penulis) {
    HasilCari h;
    if (judul.empty()) {
        h.jejak.push_back("judul kosong: tidak ada yang dicari");
        return h;
    }

    const std::string key = buat_key(judul, penulis);
    h.jejak.push_back("key cache: " + key);

    // Langkah 1: cache_metadata, dicari lewat key judul+penulis.
    std::optional<std::string> c_penerbit, c_sumber, c_isbn, c_status;
    bool ada_baris = false;
    {
        auto st = siapkan(db,
            "SELECT penerbit, sumber, isbn, status FROM cache_metadata "
            "WHERE judul_penulis_key = ?");
        bind_teks(st.get(), 1, key);
        const int rc = sqlite3_step(st.get());
        if (rc == SQLITE_ROW) {
            ada_baris = true;
            c_penerbit = kolom_opsional(st.get(), 0);
            c_sumber   = kolom_opsional(st.get(), 1);
            c_isbn     = kolom_opsional(st.get(), 2);
            c_status   = kolom_opsional(st.get(), 3);
        } else if (rc != SQLITE_DONE) {
            gagal(db, "baca cache_metadata");
        }
    }

    // Sama dengan notebook: baris cache hanya dipakai kalau penerbitnya terisi.
    if (ada_baris && terisi(c_penerbit)) {
        h.ketemu = true;
        h.penerbit = *c_penerbit;
        h.sumber = atau_kosong(c_sumber) + "_cache";
        h.status_cache = atau_kosong(c_status);
        h.isbn = atau_kosong(c_isbn);
        h.jejak.push_back("cache_metadata: ketemu (status: " +
                          (h.status_cache.empty() ? std::string("-") : h.status_cache) + ")");
        if (h.status_cache == "ditolak")
            h.jejak.push_back("PERINGATAN: entri ini berstatus ditolak, tapi lengkapi_metadata "
                              "di notebook tidak memeriksa status dan tetap memakainya");

        if (h.isbn.empty()) {
            // Notebook: ISBN kosong di cache dicari di katalog (judul dan penulis persis).
            std::optional<std::string> pen;
            if (!penulis.empty()) pen = penulis;  // penulis kosong = NULL, tidak pernah cocok
            auto k = siapkan(db,
                "SELECT isbn FROM katalog WHERE judul = ?1 AND penulis = ?2 "
                "AND isbn IS NOT NULL LIMIT 1");
            bind_teks(k.get(), 1, judul);
            bind_opsional(k.get(), 2, pen);
            const int rc = sqlite3_step(k.get());
            if (rc == SQLITE_ROW) {
                h.isbn = atau_kosong(kolom_opsional(k.get(), 0));
                h.jejak.push_back("isbn kosong di cache, diambil dari katalog");
            } else if (rc == SQLITE_DONE) {
                h.jejak.push_back("isbn kosong di cache, katalog juga belum punya");
            } else {
                gagal(db, "baca katalog");
            }
        }
        return h;
    }
    h.jejak.push_back(ada_baris ? "cache_metadata: ada baris tapi penerbit kosong, dilewati"
                                : "cache_metadata: tidak ada");

    // Langkah 2: rag_manual, fuzzy seperti cari_di_rag_manual di notebook:
    // skor difflib atas "judul penulis" (huruf kecil), ambil yang tertinggi, minimal 0.75.
    {
        const std::u32string query = normalisasi_kecil(judul + " " + penulis);
        auto st = siapkan(db, "SELECT judul, penulis, penerbit, isbn FROM rag_manual");
        double skor_terbaik = 0.0;
        bool ada_kandidat = false;
        std::string j_cocok, p_cocok, penerbit_cocok, isbn_cocok;
        int rc;
        while ((rc = sqlite3_step(st.get())) == SQLITE_ROW) {
            const std::string j = atau_kosong(kolom_opsional(st.get(), 0));
            const std::string p = atau_kosong(kolom_opsional(st.get(), 1));
            const double skor = rasio_difflib(query, normalisasi_kecil(j + " " + p));
            if (skor > skor_terbaik) {  // strict: kalau seri, yang lebih dulu menang (seperti notebook)
                skor_terbaik = skor;
                ada_kandidat = true;
                j_cocok = j;
                p_cocok = p;
                penerbit_cocok = atau_kosong(kolom_opsional(st.get(), 2));
                isbn_cocok = atau_kosong(kolom_opsional(st.get(), 3));
            }
        }
        if (rc != SQLITE_DONE) gagal(db, "baca rag_manual");

        std::ostringstream skor_teks;
        skor_teks << std::fixed << std::setprecision(3) << skor_terbaik;

        if (ada_kandidat && skor_terbaik >= kAmbangMirip) {
            h.ketemu = true;
            h.penerbit = penerbit_cocok;
            h.isbn = isbn_cocok;
            h.sumber = "rag_manual";
            h.jejak.push_back("rag_manual: ketemu (skor " + skor_teks.str() + "): " + j_cocok +
                              " | " + p_cocok);
            if (h.penerbit.empty()) h.jejak.push_back("catatan: penerbit di rag_manual kosong");
            return h;
        }
        h.jejak.push_back(ada_kandidat
            ? "rag_manual: tidak ada yang cukup mirip (skor tertinggi " + skor_teks.str() +
              " < 0.75): " + j_cocok + " | " + p_cocok
            : "rag_manual: kosong atau tidak ada yang mirip");
    }
    h.jejak.push_back("langkah berikutnya di notebook: Open Library (tidak dijalankan di C++)");
    return h;
}

HasilCari cari_dari_isbn_lokal(sqlite3* db, const std::string& isbn_mentah) {
    HasilCari h;
    const std::string isbn = bersihkan_isbn(isbn_mentah);
    h.isbn = isbn;
    if (isbn.empty()) {
        h.jejak.push_back("isbn kosong: tidak ada yang dicari");
        return h;
    }
    h.jejak.push_back("isbn bersih: " + isbn);

    // Langkah 1: cache_metadata (lewat isbn) DAN katalog (lewat isbn). Keduanya harus ada,
    // seperti di notebook.
    std::optional<std::string> c_penerbit;
    bool ada_cache = false;
    {
        auto st = siapkan(db, "SELECT penerbit FROM cache_metadata WHERE isbn = ? LIMIT 1");
        bind_teks(st.get(), 1, isbn);
        const int rc = sqlite3_step(st.get());
        if (rc == SQLITE_ROW) {
            ada_cache = true;
            c_penerbit = kolom_opsional(st.get(), 0);
        } else if (rc != SQLITE_DONE) {
            gagal(db, "baca cache_metadata");
        }
    }
    if (ada_cache) {
        auto k = siapkan(db, "SELECT judul, penulis FROM katalog WHERE isbn = ? LIMIT 1");
        bind_teks(k.get(), 1, isbn);
        const int rc = sqlite3_step(k.get());
        if (rc == SQLITE_ROW) {
            h.ketemu = true;
            h.judul = atau_kosong(kolom_opsional(k.get(), 0));
            h.penulis = atau_kosong(kolom_opsional(k.get(), 1));
            h.penerbit = atau_kosong(c_penerbit);
            h.sumber = "open_library_isbn_cache";
            h.jejak.push_back("cache_metadata + katalog: ketemu");
            return h;
        }
        if (rc != SQLITE_DONE) gagal(db, "baca katalog");
        h.jejak.push_back("cache_metadata: ada, tapi katalog belum punya ISBN ini");
    } else {
        h.jejak.push_back("cache_metadata: tidak ada");
    }

    h.jejak.push_back("Open Library: dilewati (C++ tidak punya akses jaringan); di notebook "
                      "langkah ini terjadi SEBELUM rag_manual");

    // Langkah 2: rag_manual, exact match ISBN. Di notebook ini hanya dipakai kalau
    // Open Library gagal.
    {
        auto st = siapkan(db,
            "SELECT judul, penulis, penerbit FROM rag_manual WHERE isbn = ? LIMIT 1");
        bind_teks(st.get(), 1, isbn);
        const int rc = sqlite3_step(st.get());
        if (rc == SQLITE_ROW) {
            h.ketemu = true;
            h.judul = atau_kosong(kolom_opsional(st.get(), 0));
            h.penulis = atau_kosong(kolom_opsional(st.get(), 1));
            h.penerbit = atau_kosong(kolom_opsional(st.get(), 2));
            h.sumber = "rag_manual";
            h.jejak.push_back("rag_manual: ketemu (ISBN persis). Di notebook baru dipakai "
                              "kalau Open Library tidak punya data");
            return h;
        }
        if (rc != SQLITE_DONE) gagal(db, "baca rag_manual");
    }
    h.jejak.push_back("rag_manual: tidak ada");
    return h;
}

void cetak_hasil(const HasilCari& h) {
    const auto tampil = [](const std::string& s) { return s.empty() ? std::string("-") : s; };

    std::cout << "Jejak:\n";
    for (const auto& j : h.jejak) std::cout << "  - " << j << "\n";

    if (!h.ketemu) {
        std::cout << "Hasil: tidak ketemu di DB lokal\n";
        return;
    }
    std::cout << "Hasil:\n";
    if (!h.judul.empty())   std::cout << "  judul    : " << h.judul << "\n";
    if (!h.penulis.empty()) std::cout << "  penulis  : " << h.penulis << "\n";
    std::cout << "  penerbit : " << tampil(h.penerbit) << "\n"
              << "  isbn     : " << tampil(h.isbn) << "\n"
              << "  sumber   : " << h.sumber << "\n";
    if (!h.status_cache.empty()) std::cout << "  status   : " << h.status_cache << "\n";
}
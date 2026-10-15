// cache_ttl.cpp
#include "cache/cache_ttl.h"

#include <iostream>
#include <string>

#include "db.h"

namespace {

struct Aturan {
    const char* tabel;
    int hari_bawaan;
};

// cache_kosong sengaja singkat: buku yang belum ketemu hari ini bisa saja sudah ada besok.
constexpr Aturan kAturan[] = {{"cache_isbn", 90}, {"cache_kosong", 7}, {"cache_scan", 90}};

const char* kKondisi = "waktu_masuk IS NOT NULL AND waktu_masuk < datetime('now', '-' || ?1 || ' days')";

long long hitung(sqlite3* db, const std::string& sql, int hari) {
    auto st = siapkan(db, sql.c_str());
    bind_teks(st.get(), 1, std::to_string(hari));
    if (sqlite3_step(st.get()) != SQLITE_ROW) gagal(db, "hitung cache");
    return sqlite3_column_int64(st.get(), 0);
}

}  // namespace

void cache_bersihkan(sqlite3* db, int hari, bool yes) {
    long long lama_total = 0;
    long long lama[3];
    int umur[3];
    for (int i = 0; i < 3; ++i) {
        umur[i] = hari > 0 ? hari : kAturan[i].hari_bawaan;
        const std::string tabel = kAturan[i].tabel;
        const long long total = hitung(db, "SELECT COUNT(*) FROM " + tabel + " WHERE ?1 = ?1", 0);
        lama[i] = hitung(db, "SELECT COUNT(*) FROM " + tabel + " WHERE " + kKondisi, umur[i]);
        lama_total += lama[i];
        std::cout << tabel << ": " << total << " baris, " << lama[i] << " lebih tua dari " << umur[i] << " hari\n";
    }
    if (lama_total == 0) {
        std::cout << "Tidak ada yang dihapus.\n";
        return;
    }
    if (!yes) {
        std::cout << "Hapus " << lama_total << " baris? (y/N): ";
        std::string jawab;
        std::getline(std::cin, jawab);
        if (jawab != "y" && jawab != "Y") {
            std::cout << "Dibatalkan.\n";
            return;
        }
    }
    jalankan(db, "BEGIN");
    try {
        for (int i = 0; i < 3; ++i) {
            auto st = siapkan(db, (std::string("DELETE FROM ") + kAturan[i].tabel + " WHERE " + kKondisi).c_str());
            bind_teks(st.get(), 1, std::to_string(umur[i]));
            if (sqlite3_step(st.get()) != SQLITE_DONE) gagal(db, "hapus cache");
            std::cout << "[OK] " << kAturan[i].tabel << ": " << sqlite3_changes(db) << " baris dihapus\n";
        }
        jalankan(db, "COMMIT");
    } catch (...) {
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
}
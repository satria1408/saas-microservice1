#include "cache/isbn/isbn_hapus.h"

#include <iostream>
#include <stdexcept>

#include "db.h"
#include "util.h"

namespace {

bool ada_baris(sqlite3* db, const char* sql, const std::string& isbn) {
    auto st = siapkan(db, sql);
    bind_teks(st.get(), 1, isbn);
    return sqlite3_step(st.get()) == SQLITE_ROW;
}

// Hapus dari kedua tabel dalam satu transaksi; mengembalikan jumlah baris yang terbuang.
long long hapus_isbn(sqlite3* db, const std::string& isbn) {
    long long total = 0;
    jalankan(db, "BEGIN");
    try {
        for (const char* sql : {"DELETE FROM cache_isbn WHERE isbn = ?", "DELETE FROM cache_kosong WHERE isbn = ?"}) {
            auto st = siapkan(db, sql);
            bind_teks(st.get(), 1, isbn);
            if (sqlite3_step(st.get()) != SQLITE_DONE) gagal(db, "hapus cache");
            total += sqlite3_changes(db);
        }
        jalankan(db, "COMMIT");
    } catch (...) {
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
    return total;
}

}  // namespace

void isbn_hapus(sqlite3* db, const std::string& isbn_mentah, bool yes) {
    const auto isbn = bersihkan_isbn(isbn_mentah);
    if (!isbn) throw std::runtime_error("isbn kosong");

    const bool di_isbn = ada_baris(db, "SELECT 1 FROM cache_isbn WHERE isbn = ?", *isbn);
    const bool di_kosong = ada_baris(db, "SELECT 1 FROM cache_kosong WHERE isbn = ?", *isbn);
    if (!di_isbn && !di_kosong)
        throw std::runtime_error("ISBN tidak ada di cache_isbn maupun cache_kosong: " + *isbn);

    std::cout << "Akan dihapus untuk ISBN " << *isbn << ":\n"
              << (di_isbn ? "  - entri cache_isbn\n" : "") << (di_kosong ? "  - catatan cache_kosong\n" : "");
    if (!yes) {
        std::cout << "Lanjut hapus? (y/N): ";
        std::string jawab;
        std::getline(std::cin, jawab);
        if (jawab != "y" && jawab != "Y") {
            std::cout << "Dibatalkan.\n";
            return;
        }
    }
    std::cout << "[OK] " << hapus_isbn(db, *isbn) << " baris dihapus\n";
}

void bersihkan_cache_setelah_inti(sqlite3* cache, const std::vector<std::string>& daftar_isbn) {
    if (!cache || daftar_isbn.empty()) return;
    try {
        long long total = 0;
        for (const auto& isbn : daftar_isbn) total += hapus_isbn(cache, isbn);
        if (total > 0) std::cout << "[cache] " << total << " entri cache untuk ISBN yang baru masuk inti dibuang\n";
    } catch (const std::exception& e) {
        std::cerr << "[WARN] gagal membersihkan cache (data inti sudah tersimpan): " << e.what() << "\n";
    }
}
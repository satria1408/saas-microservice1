#include "cache/scan/scan_bersihkan.h"

#include <iostream>
#include <stdexcept>
#include <string>

#include "db.h"

namespace {

long long hitung(sqlite3* db, const std::string& sql, const std::string* param) {
    auto st = siapkan(db, sql.c_str());
    if (param) bind_teks(st.get(), 1, *param);
    if (sqlite3_step(st.get()) != SQLITE_ROW) gagal(db, "hitung cache_scan");
    return sqlite3_column_int64(st.get(), 0);
}

}  // namespace

void scan_bersihkan(sqlite3* db, int hari, bool yes) {
    if (hari < 1) throw std::runtime_error("cache scan bersihkan butuh --hari N (N minimal 1)");

    // Baris tanpa waktu_masuk (NULL) sengaja tidak masuk kondisi: tidak pernah dihapus.
    const std::string kondisi =
        "waktu_masuk IS NOT NULL AND waktu_masuk < datetime('now', '-' || ?1 || ' days')";
    const std::string batas = std::to_string(hari);

    const long long total = hitung(db, "SELECT COUNT(*) FROM cache_scan", nullptr);
    const long long tanpa_waktu =
        hitung(db, "SELECT COUNT(*) FROM cache_scan WHERE waktu_masuk IS NULL", nullptr);
    const long long lama =
        hitung(db, "SELECT COUNT(*) FROM cache_scan WHERE " + kondisi, &batas);

    std::cout << "Total baris             : " << total << "\n"
              << "Lebih tua dari " << hari << " hari  : " << lama << "\n"
              << "Tanpa waktu (dilindungi): " << tanpa_waktu << "\n";

    if (lama == 0) {
        std::cout << "Tidak ada yang dihapus.\n";
        return;
    }

    if (!yes) {
        std::cout << "Hapus " << lama << " baris? (y/N): ";
        std::string jawab;
        std::getline(std::cin, jawab);
        if (jawab != "y" && jawab != "Y") {
            std::cout << "Dibatalkan.\n";
            return;
        }
    }

    auto st = siapkan(db, ("DELETE FROM cache_scan WHERE " + kondisi).c_str());
    bind_teks(st.get(), 1, batas);
    if (sqlite3_step(st.get()) != SQLITE_DONE) gagal(db, "hapus cache_scan");
    std::cout << "[OK] " << sqlite3_changes(db) << " baris dihapus\n";
}
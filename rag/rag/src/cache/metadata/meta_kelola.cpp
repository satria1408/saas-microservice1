#include "meta_kelola.h"

#include <iostream>
#include <stdexcept>

#include "meta_create.h"

void meta_set_status(sqlite3* db, const std::string& key, const std::string& status) {
    if (!status_meta_valid(status))
        throw std::runtime_error("status harus: baru, ditinjau, atau ditolak");
    if (status == "dipromosikan")
        throw std::runtime_error("status 'dipromosikan' hanya diatur lewat perintah promosi");

    auto st = siapkan(db, "UPDATE cache_metadata SET status=? WHERE judul_penulis_key=?");
    bind_teks(st.get(), 1, status);
    bind_teks(st.get(), 2, key);
    if (sqlite3_step(st.get()) != SQLITE_DONE) gagal(db, "ubah status");

    if (sqlite3_changes(db) == 0) std::cout << "Key '" << key << "' tidak ada.\n";
    else std::cout << "Status '" << key << "' -> " << status << "\n";
}

void meta_hapus(sqlite3* db, const std::string& key, bool tanpa_tanya) {
    auto cari = siapkan(db,
        "SELECT COALESCE(judul_asli, judul_penulis_key), penerbit"
        " FROM cache_metadata WHERE judul_penulis_key=?");
    bind_teks(cari.get(), 1, key);
    if (sqlite3_step(cari.get()) != SQLITE_ROW) {
        std::cout << "Key '" << key << "' tidak ada.\n";
        return;
    }
    std::string judul = kolom_teks(cari.get(), 0), penerbit = kolom_teks(cari.get(), 1);
    cari.reset();

    if (!tanpa_tanya) {
        std::cout << "Hapus cache '" << judul << "' (penerbit: " << penerbit << ")? (y/N) ";
        std::string jawab;
        std::getline(std::cin, jawab);
        if (jawab != "y" && jawab != "Y") {
            std::cout << "Dibatalkan.\n";
            return;
        }
    }
    auto del = siapkan(db, "DELETE FROM cache_metadata WHERE judul_penulis_key=?");
    bind_teks(del.get(), 1, key);
    if (sqlite3_step(del.get()) != SQLITE_DONE) gagal(db, "hapus");
    std::cout << "Terhapus (" << sqlite3_changes(db) << " baris).\n";
}

// ?1 = jumlah hari (0 = kriteria umur dimatikan).
// waktu_masuk NULL (baris lama dari notebook) tidak pernah lolos kriteria umur.
static const char* KRITERIA =
    " WHERE COALESCE(status,'baru') IN ('dipromosikan','ditolak')"
    "    OR (?1 > 0 AND COALESCE(status,'baru')='baru' AND waktu_masuk IS NOT NULL"
    "        AND waktu_masuk < datetime('now', '-' || ?1 || ' days'))";

void meta_bersihkan(sqlite3* db, int hari, bool tanpa_tanya) {
    if (hari < 0) throw std::runtime_error("--hari tidak boleh negatif");

    long long jumlah = 0;
    {
        auto hitung = siapkan(db,
            (std::string("SELECT COUNT(*) FROM cache_metadata") + KRITERIA).c_str());
        sqlite3_bind_int(hitung.get(), 1, hari);
        if (sqlite3_step(hitung.get()) != SQLITE_ROW) gagal(db, "menghitung");
        jumlah = sqlite3_column_int64(hitung.get(), 0);
    }
    if (jumlah == 0) {
        std::cout << "Tidak ada entri yang perlu dibersihkan.\n";
        return;
    }

    if (!tanpa_tanya) {
        std::cout << "Akan menghapus " << jumlah << " entri cache. Lanjut? (y/N) ";
        std::string jawab;
        std::getline(std::cin, jawab);
        if (jawab != "y" && jawab != "Y") {
            std::cout << "Dibatalkan.\n";
            return;
        }
    }
    auto del = siapkan(db,
        (std::string("DELETE FROM cache_metadata") + KRITERIA).c_str());
    sqlite3_bind_int(del.get(), 1, hari);
    if (sqlite3_step(del.get()) != SQLITE_DONE) gagal(db, "membersihkan");
    std::cout << "Terhapus " << sqlite3_changes(db) << " entri.\n";
}
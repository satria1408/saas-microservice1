#include "katalog/katalog_read.h"

#include <iostream>
#include <stdexcept>

#include "db.h"

bool status_katalog_valid(const std::string& s) {
    return s == "otomatis" || s == "terkonfirmasi";
}

void katalog_daftar(sqlite3* db, const std::string& status, const std::string& kata) {
    auto st = siapkan(db,
        "SELECT id, judul, penulis, penerbit, isbn, stok, status_konfirmasi, kategori "
        "FROM katalog "
        "WHERE (?1 = '' OR status_konfirmasi = ?1) "
        "AND (?2 = '' OR judul LIKE '%' || ?2 || '%' OR penulis LIKE '%' || ?2 || '%') "
        "ORDER BY id");
    bind_teks(st.get(), 1, status);
    bind_teks(st.get(), 2, kata);

    int n = 0, rc;
    while ((rc = sqlite3_step(st.get())) == SQLITE_ROW) {
        std::cout << "#" << kolom_teks(st.get(), 0) << " " << kolom_teks(st.get(), 1)
                  << " | " << kolom_teks(st.get(), 2)
                  << " | " << kolom_teks(st.get(), 3)
                  << " | ISBN: " << kolom_teks(st.get(), 4)
                  << " | stok " << kolom_teks(st.get(), 5)
                  << " | " << kolom_teks(st.get(), 6)
                  << " | " << kolom_teks(st.get(), 7) << "\n";
        ++n;
    }
    if (rc != SQLITE_DONE) gagal(db, "baca katalog");
    std::cout << n << " baris\n";
}

void katalog_tampil(sqlite3* db, long long id) {
    auto st = siapkan(db,
        "SELECT id, judul, penulis, penerbit, penerbit_sumber, stok, "
        "status_konfirmasi, waktu_masuk, isbn, kategori FROM katalog WHERE id = ?");
    sqlite3_bind_int64(st.get(), 1, id);

    int rc = sqlite3_step(st.get());
    if (rc == SQLITE_DONE) throw std::runtime_error("katalog id tidak ditemukan: " + std::to_string(id));
    if (rc != SQLITE_ROW) gagal(db, "baca katalog");

    std::cout << "id              : " << kolom_teks(st.get(), 0) << "\n"
              << "judul           : " << kolom_teks(st.get(), 1) << "\n"
              << "penulis         : " << kolom_teks(st.get(), 2) << "\n"
              << "penerbit        : " << kolom_teks(st.get(), 3) << "\n"
              << "penerbit_sumber : " << kolom_teks(st.get(), 4) << "\n"
              << "stok            : " << kolom_teks(st.get(), 5) << "\n"
              << "status          : " << kolom_teks(st.get(), 6) << "\n"
              << "waktu_masuk     : " << kolom_teks(st.get(), 7) << "\n"
              << "isbn            : " << kolom_teks(st.get(), 8) << "\n"
              << "kategori        : " << kolom_teks(st.get(), 9) << "\n";
}
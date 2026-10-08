#include "cache/scan/scan_create.h"

#include <iostream>
#include <optional>
#include <stdexcept>

#include "db.h"

namespace {
std::optional<std::string> kosong_jadi_null(const std::string& s) {
    if (s.empty()) return std::nullopt;
    return s;
}
}  

void scan_tambah(sqlite3* db, const std::string& hash, const std::string& judul,
                 const std::string& penulis, const std::string& kategori) {
    if (hash.empty()) throw std::runtime_error("hash tidak boleh kosong");

    auto st = siapkan(db,
        "INSERT INTO cache_scan (hash_gambar, judul, penulis, kategori, waktu_masuk) "
        "VALUES (?, ?, ?, ?, datetime('now')) "
        "ON CONFLICT(hash_gambar) DO UPDATE SET "
        "judul = excluded.judul, penulis = excluded.penulis, "
        "kategori = excluded.kategori, waktu_masuk = excluded.waktu_masuk");
    bind_teks(st.get(), 1, hash);
    bind_opsional(st.get(), 2, kosong_jadi_null(judul));
    bind_opsional(st.get(), 3, kosong_jadi_null(penulis));
    bind_opsional(st.get(), 4, kosong_jadi_null(kategori));

    if (sqlite3_step(st.get()) != SQLITE_DONE) gagal(db, "simpan cache_scan");
    std::cout << "[OK] cache scan tersimpan: " << hash << "\n";
}
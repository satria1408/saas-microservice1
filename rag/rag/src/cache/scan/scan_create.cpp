#include "cache/scan/scan_create.h"

#include <iostream>
#include <optional>
#include <stdexcept>

#include "db.h"
#include "util.h"

namespace {
std::optional<std::string> kosong_jadi_null(const std::string& s) {
    if (s.empty()) return std::nullopt;
    return s;
}
}  // namespace

void scan_tambah(sqlite3* db, const std::string& hash, const std::string& judul,
                 const std::string& penulis, const std::string& kategori,
                 const std::string& isbn_mentah) {
    if (hash.empty()) throw std::runtime_error("hash tidak boleh kosong");

    // Jaring kedua: validasi_bentuk sudah mengecek ini sebelum backup.
    std::optional<std::string> isbn;
    if (!trim(isbn_mentah).empty()) {
        isbn = bersihkan_isbn(isbn_mentah);
        if (!isbn || !isbn13_valid(*isbn))  // <- berubah: sebelumnya ean13_valid
            throw std::runtime_error("ISBN '" + trim(isbn_mentah) +
                                     "' bukan ISBN-13 yang valid (awalan 978/979 dan checksum benar)");
    }

    // isbn memakai COALESCE: menyimpan ulang hash yang sama tanpa ISBN tidak
    // menghapus barcode yang sudah tercatat.
    auto st = siapkan(db,
        "INSERT INTO cache_scan (hash_gambar, judul, penulis, kategori, isbn, waktu_masuk) "
        "VALUES (?, ?, ?, ?, ?, datetime('now')) "
        "ON CONFLICT(hash_gambar) DO UPDATE SET "
        "judul = excluded.judul, penulis = excluded.penulis, "
        "kategori = excluded.kategori, isbn = COALESCE(excluded.isbn, isbn), "
        "waktu_masuk = excluded.waktu_masuk");
    bind_teks(st.get(), 1, hash);
    bind_opsional(st.get(), 2, kosong_jadi_null(judul));
    bind_opsional(st.get(), 3, kosong_jadi_null(penulis));
    bind_opsional(st.get(), 4, kosong_jadi_null(kategori));
    bind_opsional(st.get(), 5, isbn);

    if (sqlite3_step(st.get()) != SQLITE_DONE) gagal(db, "simpan cache_scan");
    std::cout << "[OK] cache scan tersimpan: " << hash << "\n";
}
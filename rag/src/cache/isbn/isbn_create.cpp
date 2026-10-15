#include "cache/isbn/isbn_create.h"

#include <iostream>
#include <optional>
#include <stdexcept>

#include "db.h"
#include "util.h"

namespace {
std::optional<std::string> kosong_jadi_null(const std::string& s) {
    if (trim(s).empty()) return std::nullopt;
    return trim(s);
}
}  // namespace

void isbn_simpan(sqlite3* db, const std::string& isbn_mentah, const std::string& judul,
                 const std::string& penulis, const std::string& penerbit,
                 const std::string& sumber) {
    const auto isbn = bersihkan_isbn(isbn_mentah);
    if (!isbn || !isbn13_valid(*isbn))
        throw std::runtime_error("ISBN '" + trim(isbn_mentah) +
                                 "' bukan ISBN-13 yang valid (awalan 978/979 dan checksum benar)");
    if (trim(judul).empty()) throw std::runtime_error("cache add butuh <judul>");

    long long kosong_terhapus = 0;
    jalankan(db, "BEGIN");
    try {
        {
            auto st = siapkan(db,
                "INSERT INTO cache_isbn (isbn, judul, penulis, penerbit, sumber, waktu_masuk) "
                "VALUES (?1, ?2, ?3, ?4, ?5, datetime('now')) "
                "ON CONFLICT(isbn) DO UPDATE SET "
                "judul = COALESCE(excluded.judul, judul), "
                "penulis = COALESCE(excluded.penulis, penulis), "
                "penerbit = COALESCE(excluded.penerbit, penerbit), "
                "sumber = COALESCE(excluded.sumber, sumber), "
                "waktu_masuk = datetime('now')");
            bind_teks(st.get(), 1, *isbn);
            bind_opsional(st.get(), 2, kosong_jadi_null(judul));
            bind_opsional(st.get(), 3, kosong_jadi_null(penulis));
            bind_opsional(st.get(), 4, kosong_jadi_null(penerbit));
            bind_opsional(st.get(), 5, kosong_jadi_null(sumber));
            if (sqlite3_step(st.get()) != SQLITE_DONE) gagal(db, "simpan cache_isbn");
        }
        {
            auto st = siapkan(db, "DELETE FROM cache_kosong WHERE isbn = ?");
            bind_teks(st.get(), 1, *isbn);
            if (sqlite3_step(st.get()) != SQLITE_DONE) gagal(db, "hapus cache_kosong");
            kosong_terhapus = sqlite3_changes(db);
        }
        jalankan(db, "COMMIT");
    } catch (...) {
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
    std::cout << "[OK] cache_isbn tersimpan: " << *isbn
              << (kosong_terhapus ? " (catatan 'kosong' untuk ISBN ini dihapus)" : "") << "\n";
}
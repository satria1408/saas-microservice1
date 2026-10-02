#include "meta_read.h"

#include <iostream>
#include <stdexcept>

#include "meta_create.h"
#include "util.h"

void meta_daftar(sqlite3* db, const std::string& status, const std::string& kata) {
    if (!status.empty() && !status_meta_valid(status))
        throw std::runtime_error("status harus: baru, ditinjau, dipromosikan, atau ditolak");

    const char* sql =
        "SELECT judul_penulis_key, COALESCE(judul_asli, judul_penulis_key), penulis_asli,"
        " penerbit, isbn, sumber, COALESCE(status,'baru'), waktu_masuk"
        " FROM cache_metadata"
        " WHERE (?1='' OR COALESCE(status,'baru')=?1)"
        "   AND (?2='' OR LOWER(COALESCE(judul_asli,judul_penulis_key)) LIKE ?2"
        "        OR LOWER(COALESCE(penulis_asli,'')) LIKE ?2"
        "        OR LOWER(COALESCE(penerbit,'')) LIKE ?2"
        "        OR COALESCE(isbn,'') LIKE ?2)"
        " ORDER BY rowid";

    auto st = siapkan(db, sql);
    bind_teks(st.get(), 1, status);
    bind_teks(st.get(), 2, kata.empty() ? std::string() : "%" + huruf_kecil(kata) + "%");

    int n = 0, rc;
    while ((rc = sqlite3_step(st.get())) == SQLITE_ROW) {
        std::cout << "[" << kolom_teks(st.get(), 6) << "] "
                  << kolom_teks(st.get(), 1) << " | " << kolom_teks(st.get(), 2) << " | "
                  << kolom_teks(st.get(), 3) << " | ISBN: " << kolom_teks(st.get(), 4) << " | "
                  << kolom_teks(st.get(), 5) << " | " << kolom_teks(st.get(), 7) << "\n"
                  << "         key: " << kolom_teks(st.get(), 0) << "\n";
        ++n;
    }
    if (rc != SQLITE_DONE) gagal(db, "membaca cache_metadata");
    std::cout << "\n" << n << " baris.\n";
}
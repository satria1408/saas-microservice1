#include "delete.h"

#include <iostream>
#include <string>

void hapus(sqlite3* db, long long id, bool tanpa_tanya) {
    auto cari = siapkan(db, "SELECT judul, penulis FROM rag_manual WHERE id=?");
    sqlite3_bind_int64(cari.get(), 1, id);
    if (sqlite3_step(cari.get()) != SQLITE_ROW) {
        std::cout << "ID " << id << " tidak ada.\n";
        return;
    }
    std::string judul = kolom_teks(cari.get(), 0), penulis = kolom_teks(cari.get(), 1);
    cari.reset();

    if (!tanpa_tanya) {
        std::cout << "Hapus #" << id << " '" << judul << "' - " << penulis << "? (y/N) ";
        std::string jawab;
        std::getline(std::cin, jawab);
        if (jawab != "y" && jawab != "Y") {
            std::cout << "Dibatalkan.\n";
            return;
        }
    }
    auto del = siapkan(db, "DELETE FROM rag_manual WHERE id=?");
    sqlite3_bind_int64(del.get(), 1, id);
    if (sqlite3_step(del.get()) != SQLITE_DONE) gagal(db, "hapus");
    std::cout << "Terhapus (" << sqlite3_changes(db) << " baris).\n";
}

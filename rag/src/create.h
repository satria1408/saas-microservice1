#pragma once
// CREATE: tambah satu buku (add) atau banyak sekaligus dari CSV (import).
#include <string>

#include "db.h"

enum class Hasil { Baru, Update, Lewati };

// Menyimpan satu buku. Kalau judul+penulis sudah ada, barisnya di-update (upsert).
// Prepared statement dibuat sekali di konstruktor lalu dipakai ulang.
class Penyimpan {
public:
    explicit Penyimpan(sqlite3* db);

    Hasil simpan(const std::string& judul, const std::string& penulis,
                 const std::string& penerbit, const std::string& isbn_mentah,
                 const std::string& sumber, bool rinci);

private:
    sqlite3* db_;
    StmtPtr cari_, upd_, ins_;
};

void tambah(sqlite3* db, const std::string& judul, const std::string& penulis,
            const std::string& penerbit, const std::string& isbn);

// pakai_transaksi=false hanya untuk membandingkan kecepatan.
void impor(sqlite3* db, const std::string& path, bool pakai_transaksi, bool rinci);

#pragma once
// CREATE: tambah satu buku (add) atau banyak sekaligus dari CSV (import).
#include <string>
#include <vector>

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

    const std::vector<std::string>& isbn_masuk() const { return isbn_masuk_; }

private:
    sqlite3* db_;
    StmtPtr cari_isbn_, cari_judul_, upd_, ins_;
    std::vector<std::string> isbn_masuk_;
};

void tambah(sqlite3* db, const std::string& judul, const std::string& penulis,
            const std::string& penerbit, const std::string& isbn,
            std::vector<std::string>* isbn_masuk = nullptr);

// pakai_transaksi=false hanya untuk membandingkan kecepatan.
void impor(sqlite3* db, const std::string& path, bool pakai_transaksi, bool rinci,
           std::vector<std::string>* isbn_masuk = nullptr);
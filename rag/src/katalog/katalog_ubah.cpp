#include "katalog/katalog_ubah.h"

#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include "crud/create.h"
#include "db.h"
#include "util.h"

namespace {

bool dilewati(const std::string& s) { return s.empty() || s == "-"; }

std::string teks_atau_kosong(sqlite3_stmt* st, int i) {
    return sqlite3_column_type(st, i) == SQLITE_NULL ? std::string() : kolom_teks(st, i);
}

std::string tampil(const std::string& s) { return s.empty() ? std::string("-") : s; }

}  // namespace

// Konfirmasi = katalog (staging) ditandai terkonfirmasi DAN datanya masuk ke rag_manual
// (data inti). Keduanya dalam satu transaksi.
void katalog_konfirmasi(sqlite3* db, long long id, const std::string& penerbit,
                        const std::string& isbn, const std::string& stok, bool yes,
                        std::vector<std::string>* isbn_masuk) {
    // 1. Baca baris sekarang.
    std::string judul, penulis, penerbit_lama, isbn_lama, stok_lama, status_lama;
    {
        auto cari = siapkan(db,
            "SELECT judul, penulis, penerbit, isbn, stok, status_konfirmasi "
            "FROM katalog WHERE id = ?");
        sqlite3_bind_int64(cari.get(), 1, id);
        const int rc = sqlite3_step(cari.get());
        if (rc == SQLITE_DONE) throw std::runtime_error("katalog id tidak ditemukan: " + std::to_string(id));
        if (rc != SQLITE_ROW) gagal(db, "baca katalog");
        judul         = teks_atau_kosong(cari.get(), 0);
        penulis       = teks_atau_kosong(cari.get(), 1);
        penerbit_lama = teks_atau_kosong(cari.get(), 2);
        isbn_lama     = teks_atau_kosong(cari.get(), 3);
        stok_lama     = teks_atau_kosong(cari.get(), 4);
        status_lama   = teks_atau_kosong(cari.get(), 5);
    }
    if (trim(judul).empty())
        throw std::runtime_error("katalog #" + std::to_string(id) + " tidak punya judul, tidak bisa dikonfirmasi");

    // 2. Validasi semua masukan SEBELUM ada yang berubah.
    std::optional<std::string> penerbit_baru, isbn_baru;
    std::optional<long long> stok_baru;
    if (!dilewati(penerbit)) penerbit_baru = trim(penerbit);
    if (!dilewati(isbn)) {
        const auto bersih = bersihkan_isbn(isbn);
        if (!bersih || !isbn13_valid(*bersih))
            throw std::runtime_error("ISBN tidak valid (harus ISBN-13 dengan checksum benar): " + isbn);
        isbn_baru = *bersih;
    }
    if (!dilewati(stok)) {
        if (stok.size() > 6 || stok.find_first_not_of("0123456789") != std::string::npos)
            throw std::runtime_error("stok harus angka (maksimal 6 digit): " + stok);
        stok_baru = std::stoll(stok);
    }

    const std::string penerbit_akhir = penerbit_baru ? *penerbit_baru : trim(penerbit_lama);
    const std::string isbn_akhir = isbn_baru ? *isbn_baru : isbn_lama;

    // 3. Tampilkan rencana perubahan.
    std::cout << "Katalog #" << id << ": " << judul << " | " << tampil(penulis) << "\n"
              << "  penerbit : " << tampil(penerbit_lama)
              << (penerbit_baru ? " -> " + *penerbit_baru : " (tetap)") << "\n"
              << "  isbn     : " << tampil(isbn_lama)
              << (isbn_baru ? " -> " + *isbn_baru : " (tetap)") << "\n"
              << "  stok     : " << tampil(stok_lama)
              << (stok_baru ? " -> " + std::to_string(*stok_baru) : " (tetap)") << "\n"
              << "  status   : " << tampil(status_lama) << " -> terkonfirmasi\n"
              << "  rag_manual: dimasukkan/diperbarui (penerbit: " << tampil(penerbit_akhir)
              << ", ISBN: " << tampil(isbn_akhir) << ")\n";

    if (!yes) {
        std::cout << "Lanjut? (y/N): ";
        std::string jawab;
        std::getline(std::cin, jawab);
        if (jawab != "y" && jawab != "Y") {
            std::cout << "Dibatalkan.\n";
            return;
        }
    }

    // 4. Satu transaksi: katalog diperbarui dan buku masuk rag_manual, atau tidak sama sekali.
    //    Penyimpan dibuat sebelum BEGIN supaya kesalahan prepare tidak meninggalkan transaksi.
    Penyimpan inti(db);
    jalankan(db, "BEGIN");
    try {
        {
            // COALESCE: nilai NULL berarti kolom dibiarkan.
            auto st = siapkan(db,
                "UPDATE katalog SET penerbit = COALESCE(?1, penerbit), isbn = COALESCE(?2, isbn), "
                "stok = COALESCE(?3, stok), status_konfirmasi = 'terkonfirmasi' WHERE id = ?4");
            bind_opsional(st.get(), 1, penerbit_baru);
            bind_opsional(st.get(), 2, isbn_baru);
            if (stok_baru) sqlite3_bind_int64(st.get(), 3, *stok_baru);
            else sqlite3_bind_null(st.get(), 3);
            sqlite3_bind_int64(st.get(), 4, id);
            if (sqlite3_step(st.get()) != SQLITE_DONE) gagal(db, "ubah katalog");
        }
        inti.simpan(judul, penulis, penerbit_akhir, isbn_akhir, "katalog_konfirmasi", true);
        jalankan(db, "COMMIT");
    } catch (...) {
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
    if (isbn_masuk) *isbn_masuk = inti.isbn_masuk();
    std::cout << "[OK] katalog #" << id << " terkonfirmasi\n";
}
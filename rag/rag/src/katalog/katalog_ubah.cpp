#include "katalog/katalog_ubah.h"

#include <cctype>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include "db.h"

namespace {

bool dilewati(const std::string& s) { return s.empty() || s == "-"; }

// ISBN-13: buang spasi dan tanda hubung, sisanya harus 13 digit berawalan 978/979
// dengan checksum EAN-13 yang benar. Hasilnya ISBN bersih (digit saja).
std::optional<std::string> isbn_bersih_valid(const std::string& mentah) {
    std::string d;
    for (char c : mentah) {
        if (c == '-' || c == ' ') continue;
        if (!std::isdigit(static_cast<unsigned char>(c))) return std::nullopt;
        d += c;
    }
    if (d.size() != 13) return std::nullopt;
    if (d.compare(0, 3, "978") != 0 && d.compare(0, 3, "979") != 0) return std::nullopt;
    int jumlah = 0;
    for (int i = 0; i < 12; ++i) jumlah += (d[i] - '0') * (i % 2 == 0 ? 1 : 3);
    if ((10 - jumlah % 10) % 10 != d[12] - '0') return std::nullopt;
    return d;
}

}  // namespace

void katalog_konfirmasi(sqlite3* db, long long id, const std::string& penerbit,
                        const std::string& isbn, const std::string& stok, bool yes) {
    // 1. Baca baris sekarang.
    auto cari = siapkan(db,
        "SELECT judul, penulis, penerbit, isbn, stok, status_konfirmasi "
        "FROM katalog WHERE id = ?");
    sqlite3_bind_int64(cari.get(), 1, id);
    int rc = sqlite3_step(cari.get());
    if (rc == SQLITE_DONE) throw std::runtime_error("katalog id tidak ditemukan: " + std::to_string(id));
    if (rc != SQLITE_ROW) gagal(db, "baca katalog");
    const std::string judul        = kolom_teks(cari.get(), 0);
    const std::string penulis      = kolom_teks(cari.get(), 1);
    const std::string penerbit_lama = kolom_teks(cari.get(), 2);
    const std::string isbn_lama    = kolom_teks(cari.get(), 3);
    const std::string stok_lama    = kolom_teks(cari.get(), 4);
    const std::string status_lama  = kolom_teks(cari.get(), 5);
    cari.reset();

    // 2. Validasi semua masukan SEBELUM ada yang berubah.
    std::optional<std::string> penerbit_baru, isbn_baru;
    std::optional<long long> stok_baru;
    if (!dilewati(penerbit)) penerbit_baru = penerbit;
    if (!dilewati(isbn)) {
        auto bersih = isbn_bersih_valid(isbn);
        if (!bersih)
            throw std::runtime_error("ISBN tidak valid (harus ISBN-13 dengan checksum benar): " + isbn);
        isbn_baru = *bersih;
    }
    if (!dilewati(stok)) {
        if (stok.size() > 6 || stok.find_first_not_of("0123456789") != std::string::npos)
            throw std::runtime_error("stok harus angka (maksimal 6 digit): " + stok);
        stok_baru = std::stoll(stok);
    }

    // 3. Tampilkan rencana perubahan.
    std::cout << "Katalog #" << id << ": " << judul << " | " << penulis << "\n"
              << "  penerbit : " << penerbit_lama
              << (penerbit_baru ? " -> " + *penerbit_baru : " (tetap)") << "\n"
              << "  isbn     : " << isbn_lama
              << (isbn_baru ? " -> " + *isbn_baru : " (tetap)") << "\n"
              << "  stok     : " << stok_lama
              << (stok_baru ? " -> " + std::to_string(*stok_baru) : " (tetap)") << "\n"
              << "  status   : " << status_lama << " -> terkonfirmasi\n";

    if (!yes) {
        std::cout << "Lanjut? (y/N): ";
        std::string jawab;
        std::getline(std::cin, jawab);
        if (jawab != "y" && jawab != "Y") {
            std::cout << "Dibatalkan.\n";
            return;
        }
    }

    // 4. Satu UPDATE (atomik). COALESCE: nilai NULL berarti kolom dibiarkan.
    auto st = siapkan(db,
        "UPDATE katalog SET penerbit = COALESCE(?1, penerbit), isbn = COALESCE(?2, isbn), "
        "stok = COALESCE(?3, stok), status_konfirmasi = 'terkonfirmasi' WHERE id = ?4");
    bind_opsional(st.get(), 1, penerbit_baru);
    bind_opsional(st.get(), 2, isbn_baru);
    if (stok_baru) sqlite3_bind_int64(st.get(), 3, *stok_baru);
    else sqlite3_bind_null(st.get(), 3);
    sqlite3_bind_int64(st.get(), 4, id);
    if (sqlite3_step(st.get()) != SQLITE_DONE) gagal(db, "ubah katalog");
    std::cout << "[OK] katalog #" << id << " terkonfirmasi (" << sqlite3_changes(db) << " baris)\n";
}
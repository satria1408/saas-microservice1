#include "create.h"

#include <chrono>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>

#include "csv.h"
#include "util.h"

namespace {

void ulang(sqlite3_stmt* st) {
    sqlite3_reset(st);
    sqlite3_clear_bindings(st);
}

// Jalankan SELECT id ... LIMIT 1; -1 kalau tidak ada. Statement dilepas lagi sebelum menulis.
long long ambil_id(sqlite3* db, sqlite3_stmt* st) {
    const int rc = sqlite3_step(st);
    long long id = -1;
    if (rc == SQLITE_ROW) id = sqlite3_column_int64(st, 0);
    else if (rc != SQLITE_DONE) gagal(db, "cari");
    ulang(st);
    return id;
}

}  // namespace

Penyimpan::Penyimpan(sqlite3* db)
    : db_(db),
      cari_isbn_(siapkan(db, "SELECT id FROM rag_manual WHERE isbn=? LIMIT 1")),
      // ?3 NULL (buku tanpa ISBN): cocok dengan baris mana pun. ?3 terisi: hanya baris yang
      // ISBN-nya masih kosong, supaya edisi lain (ISBN beda) tidak tertimpa.
      cari_judul_(siapkan(db, "SELECT id FROM rag_manual WHERE LOWER(judul)=LOWER(?1)"
                              " AND LOWER(COALESCE(penulis,''))=LOWER(?2)"
                              " AND (?3 IS NULL OR isbn IS NULL) ORDER BY id LIMIT 1")),
      // COALESCE(?,kolom): nilai baru kosong -> nilai lama dipertahankan
      upd_(siapkan(db, "UPDATE rag_manual SET penerbit=COALESCE(?,penerbit),"
                       " isbn=COALESCE(?,isbn), sumber=? WHERE id=?")),
      ins_(siapkan(db, "INSERT INTO rag_manual (judul, penulis, penerbit, isbn, sumber)"
                       " VALUES (?,?,?,?,?)")) {}

Hasil Penyimpan::simpan(const std::string& judul_mentah, const std::string& penulis_mentah,
                        const std::string& penerbit_mentah, const std::string& isbn_mentah,
                        const std::string& sumber, bool rinci) {
    const std::string judul = trim(judul_mentah);
    const std::string penulis = trim(penulis_mentah);
    if (judul.empty()) return Hasil::Lewati;

    std::optional<std::string> isbn = bersihkan_isbn(isbn_mentah);
    if (isbn && !isbn13_valid(*isbn)) {
        std::cerr << "[WARN] " << judul << ": ISBN '" << *isbn
                  << "' bukan ISBN-13 yang valid (978/979 + checksum), dikosongkan\n";
        isbn.reset();
    }
    std::optional<std::string> pener;
    if (!trim(penerbit_mentah).empty()) pener = trim(penerbit_mentah);

    long long id_ada = -1;
    if (isbn) {
        ulang(cari_isbn_.get());
        bind_teks(cari_isbn_.get(), 1, *isbn);
        id_ada = ambil_id(db_, cari_isbn_.get());
    }
    if (id_ada < 0) {
        ulang(cari_judul_.get());
        bind_teks(cari_judul_.get(), 1, judul);
        bind_teks(cari_judul_.get(), 2, penulis);
        bind_opsional(cari_judul_.get(), 3, isbn);
        id_ada = ambil_id(db_, cari_judul_.get());
    }

    if (id_ada >= 0) {
        ulang(upd_.get());
        bind_opsional(upd_.get(), 1, pener);
        bind_opsional(upd_.get(), 2, isbn);
        bind_teks(upd_.get(), 3, sumber);
        sqlite3_bind_int64(upd_.get(), 4, id_ada);
        if (sqlite3_step(upd_.get()) != SQLITE_DONE) gagal(db_, "update");
        ulang(upd_.get());
        if (isbn) isbn_masuk_.push_back(*isbn);
        if (rinci) std::cout << "[update] #" << id_ada << " " << judul << "\n";
        return Hasil::Update;
    }

    ulang(ins_.get());
    bind_teks(ins_.get(), 1, judul);
    bind_teks(ins_.get(), 2, penulis);
    bind_opsional(ins_.get(), 3, pener);
    bind_opsional(ins_.get(), 4, isbn);
    bind_teks(ins_.get(), 5, sumber);
    if (sqlite3_step(ins_.get()) != SQLITE_DONE) gagal(db_, "insert");
    ulang(ins_.get());
    if (isbn) isbn_masuk_.push_back(*isbn);
    if (rinci) std::cout << "[baru]   #" << sqlite3_last_insert_rowid(db_) << " " << judul << "\n";
    return Hasil::Baru;
}

void tambah(sqlite3* db, const std::string& judul, const std::string& penulis,
            const std::string& penerbit, const std::string& isbn,
            std::vector<std::string>* isbn_masuk) {
    if (trim(judul).empty()) throw std::runtime_error("add butuh minimal <judul>");
    Penyimpan p(db);
    p.simpan(judul, penulis, penerbit, isbn, "input_manual", true);
    if (isbn_masuk) *isbn_masuk = p.isbn_masuk();
}

void impor(sqlite3* db, const std::string& path, bool pakai_transaksi, bool rinci,
           std::vector<std::string>* isbn_masuk) {
    auto data = baca_csv(path);
    if (data.empty()) throw std::runtime_error("CSV kosong");

    int ij = -1, ip = -1, ipb = -1, isb = -1;
    for (size_t k = 0; k < data[0].size(); ++k) {
        std::string h = huruf_kecil(trim(data[0][k]));
        if (h == "judul") ij = static_cast<int>(k);
        else if (h == "penulis") ip = static_cast<int>(k);
        else if (h == "penerbit") ipb = static_cast<int>(k);
        else if (h == "isbn") isb = static_cast<int>(k);
    }
    if (ij < 0) throw std::runtime_error("CSV harus punya kolom 'judul' di baris pertama");

    auto ambil = [&](const std::vector<std::string>& r, int idx) {
        return (idx >= 0 && idx < static_cast<int>(r.size())) ? trim(r[idx]) : std::string();
    };

    Penyimpan p(db);
    int baru = 0, upd = 0, lewat = 0;
    auto mulai = std::chrono::steady_clock::now();

    if (pakai_transaksi) jalankan(db, "BEGIN");
    try {
        for (size_t n = 1; n < data.size(); ++n) {
            const auto& r = data[n];
            Hasil h = p.simpan(ambil(r, ij), ambil(r, ip), ambil(r, ipb), ambil(r, isb),
                               "import_csv", rinci);
            if (h == Hasil::Baru) ++baru;
            else if (h == Hasil::Update) ++upd;
            else {
                ++lewat;
                std::cerr << "[SKIP] baris " << n + 1 << ": judul kosong\n";
            }
        }
        if (pakai_transaksi) jalankan(db, "COMMIT");
    } catch (...) {
        if (pakai_transaksi) sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;  // gagal di tengah -> tidak ada yang tersimpan setengah-setengah
    }
    if (isbn_masuk) *isbn_masuk = p.isbn_masuk();  // hanya sampai sini kalau berhasil

    std::chrono::duration<double, std::milli> ms = std::chrono::steady_clock::now() - mulai;
    std::cout << "\nSelesai: " << baru << " baru, " << upd << " diupdate, " << lewat
              << " dilewati. Waktu: " << ms.count() << " ms ("
              << (pakai_transaksi ? "1 transaksi" : "tanpa transaksi") << ").\n";
}
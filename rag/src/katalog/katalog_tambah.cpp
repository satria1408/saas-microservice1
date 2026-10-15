#include "katalog/katalog_tambah.h"

#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>

#include "csv.h"
#include "db.h"
#include "util.h"

namespace {

enum class Hk { Baru, Ada, Lewati };

struct Masukan {
    std::string judul, penulis, penerbit, isbn, stok, kategori;
};

void ulang(sqlite3_stmt* st) {
    sqlite3_reset(st);
    sqlite3_clear_bindings(st);
}

long long ambil_id(sqlite3* db, sqlite3_stmt* st) {
    const int rc = sqlite3_step(st);
    long long id = -1;
    if (rc == SQLITE_ROW) id = sqlite3_column_int64(st, 0);
    else if (rc != SQLITE_DONE) gagal(db, "cari katalog");
    ulang(st);
    return id;
}

std::optional<std::string> atau_null(const std::string& s) {
    if (s.empty()) return std::nullopt;
    return s;
}

class PenyimpanKatalog {
public:
    explicit PenyimpanKatalog(sqlite3* db)
        : db_(db),
          cari_isbn_(siapkan(db, "SELECT id FROM katalog WHERE isbn=? ORDER BY id LIMIT 1")),
          cari_judul_(siapkan(db, "SELECT id FROM katalog WHERE LOWER(judul)=LOWER(?1)"
                                  " AND LOWER(COALESCE(penulis,''))=LOWER(?2) ORDER BY id LIMIT 1")),
          ins_(siapkan(db, "INSERT INTO katalog (judul, penulis, penerbit, penerbit_sumber,"
                           " stok, status_konfirmasi, isbn, kategori)"
                           " VALUES (?,?,?,?,?,'otomatis',?,?)")) {}

    Hk simpan(const Masukan& m, const std::string& sumber, bool rinci, std::string& alasan) {
        const std::string judul = trim(m.judul);
        const std::string penulis = trim(m.penulis);
        const std::string penerbit = trim(m.penerbit);
        if (judul.empty()) { alasan = "judul kosong"; return Hk::Lewati; }

        long long stok = 1;
        const std::string s = trim(m.stok);
        if (!s.empty()) {
            if (s.size() > 6 || s.find_first_not_of("0123456789") != std::string::npos) {
                alasan = "stok harus angka (maksimal 6 digit): " + s;
                return Hk::Lewati;
            }
            stok = std::stoll(s);
        }

        std::optional<std::string> isbn = bersihkan_isbn(m.isbn);
        if (isbn && !isbn13_valid(*isbn)) {
            std::cerr << "[WARN] " << judul << ": ISBN '" << *isbn
                      << "' bukan ISBN-13 yang valid (978/979 + checksum), dikosongkan\n";
            isbn.reset();
        }

        // Sudah ada? Dengan ISBN: cocokkan ISBN. Tanpa ISBN: cocokkan judul+penulis.
        long long id_ada = -1;
        if (isbn) {
            ulang(cari_isbn_.get());
            bind_teks(cari_isbn_.get(), 1, *isbn);
            id_ada = ambil_id(db_, cari_isbn_.get());
        } else {
            ulang(cari_judul_.get());
            bind_teks(cari_judul_.get(), 1, judul);
            bind_teks(cari_judul_.get(), 2, penulis);
            id_ada = ambil_id(db_, cari_judul_.get());
        }
        if (id_ada >= 0) {
            if (rinci) std::cout << "[ada]    #" << id_ada << " " << judul << " (tidak diubah)\n";
            return Hk::Ada;
        }

        ulang(ins_.get());
        bind_teks(ins_.get(), 1, judul);
        bind_opsional(ins_.get(), 2, atau_null(penulis));
        bind_opsional(ins_.get(), 3, atau_null(penerbit));
        bind_opsional(ins_.get(), 4, penerbit.empty() ? std::nullopt : atau_null(sumber));
        sqlite3_bind_int64(ins_.get(), 5, stok);
        bind_opsional(ins_.get(), 6, isbn);
        bind_opsional(ins_.get(), 7, atau_null(trim(m.kategori)));
        if (sqlite3_step(ins_.get()) != SQLITE_DONE) gagal(db_, "insert katalog");
        ulang(ins_.get());
        if (rinci)
            std::cout << "[baru]   #" << sqlite3_last_insert_rowid(db_) << " " << judul
                      << " (stok " << stok << ")\n";
        return Hk::Baru;
    }

private:
    sqlite3* db_;
    StmtPtr cari_isbn_, cari_judul_, ins_;
};

}  // namespace

void katalog_tambah(sqlite3* db, const std::string& judul, const std::string& penulis,
                    const std::string& penerbit, const std::string& isbn,
                    const std::string& stok) {
    if (trim(judul).empty()) throw std::runtime_error("katalog tambah butuh minimal <judul>");
    PenyimpanKatalog p(db);
    std::string alasan;
    const Hk h = p.simpan({judul, penulis, penerbit, isbn, stok, ""}, "input_manual", true, alasan);
    if (h == Hk::Lewati) throw std::runtime_error(alasan);
}

void katalog_impor(sqlite3* db, const std::string& path, bool rinci) {
    auto data = baca_csv(path);
    if (data.empty()) throw std::runtime_error("CSV kosong");

    int ij = -1, ip = -1, ipb = -1, isb = -1, ist = -1, ik = -1;
    for (size_t k = 0; k < data[0].size(); ++k) {
        const std::string h = huruf_kecil(trim(data[0][k]));
        const int idx = static_cast<int>(k);
        if (h == "judul") ij = idx;
        else if (h == "penulis") ip = idx;
        else if (h == "penerbit") ipb = idx;
        else if (h == "isbn") isb = idx;
        else if (h == "stok") ist = idx;
        else if (h == "kategori") ik = idx;
    }
    if (ij < 0) throw std::runtime_error("CSV harus punya kolom 'judul' di baris pertama");

    auto ambil = [&](const std::vector<std::string>& r, int idx) {
        return (idx >= 0 && idx < static_cast<int>(r.size())) ? trim(r[idx]) : std::string();
    };

    PenyimpanKatalog p(db);
    int baru = 0, ada = 0, lewat = 0;

    jalankan(db, "BEGIN");
    try {
        for (size_t n = 1; n < data.size(); ++n) {
            const auto& r = data[n];
            std::string alasan;
            const Hk h = p.simpan({ambil(r, ij), ambil(r, ip), ambil(r, ipb), ambil(r, isb),
                                   ambil(r, ist), ambil(r, ik)},
                                  "import_csv", rinci, alasan);
            if (h == Hk::Baru) ++baru;
            else if (h == Hk::Ada) ++ada;
            else {
                ++lewat;
                std::cerr << "[SKIP] baris " << n + 1 << ": " << alasan << "\n";
            }
        }
        jalankan(db, "COMMIT");
    } catch (...) {
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
    std::cout << "\nSelesai: " << baru << " baru, " << ada << " sudah ada (tidak diubah), "
              << lewat << " dilewati.\n";
}
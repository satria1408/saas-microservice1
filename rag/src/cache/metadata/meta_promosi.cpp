#include "meta_promosi.h"

#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "crud/create.h"
#include "util.h"

void meta_promosi(sqlite3* db, const std::string& key,
                  const std::string& penerbit_baru, const std::string& isbn_baru) {
     if (!trim(isbn_baru).empty()) {
        const std::optional<std::string> cek = bersihkan_isbn(isbn_baru);
    if (!cek || !ean13_valid(*cek))
            throw std::runtime_error("ISBN koreksi '" + trim(isbn_baru) +
                                     "' tidak lolos checksum EAN-13, promosi dibatalkan");
    }
    jalankan(db, "BEGIN");
    try {
        // 1. Ambil entri cache
        std::string judul, penulis, penerbit, status;
        std::optional<std::string> isbn_cache;
        {
            auto st = siapkan(db,
                "SELECT judul_asli, penulis_asli, penerbit, isbn, COALESCE(status,'baru')"
                " FROM cache_metadata WHERE judul_penulis_key=?");
            bind_teks(st.get(), 1, key);
            if (sqlite3_step(st.get()) != SQLITE_ROW)
                throw std::runtime_error("key '" + key + "' tidak ada di cache");
            if (sqlite3_column_type(st.get(), 0) == SQLITE_NULL)
                throw std::runtime_error(
                    "judul_asli kosong (baris lama dari notebook). "
                    "Tambahkan manual ke rag_manual dengan: rag add");
            judul = kolom_teks(st.get(), 0);
            if (sqlite3_column_type(st.get(), 1) != SQLITE_NULL) penulis = kolom_teks(st.get(), 1);
            if (sqlite3_column_type(st.get(), 2) != SQLITE_NULL) penerbit = kolom_teks(st.get(), 2);
            if (sqlite3_column_type(st.get(), 3) != SQLITE_NULL) isbn_cache = kolom_teks(st.get(), 3);
            status = kolom_teks(st.get(), 4);
        }
        if (status == "dipromosikan") {
            std::cout << "Sudah dipromosikan sebelumnya.\n";
            jalankan(db, "ROLLBACK");
            return;
        }
        if (status == "ditolak")
            throw std::runtime_error("entri berstatus 'ditolak'. Ubah dulu: rag cache status <key> ditinjau");

        // 2. Tentukan nilai final (argumen menimpa cache)
        std::string penerbit_final = trim(penerbit_baru).empty() ? penerbit : trim(penerbit_baru);
        std::string isbn_final = trim(isbn_baru).empty() ? (isbn_cache ? *isbn_cache : "")
                                                         : trim(isbn_baru);
        if (trim(penerbit_final).empty())
            throw std::runtime_error("tidak ada penerbit untuk dipromosikan, berikan lewat argumen");

        // 3. Masuk ke rag_manual (upsert by judul+penulis, ISBN divalidasi checksum)
        {
            Penyimpan p(db);
            p.simpan(judul, penulis, penerbit_final, isbn_final, "promosi_cache", true);
        }

        // 4. Samakan baris cache dengan hasil promosi. PENTING: urutan cek di notebook
        //    adalah cache -> rag_manual, jadi cache harus berisi nilai yang benar,
        //    kalau tidak nilai lama yang salah tetap menang.
        std::optional<std::string> isbn_valid = bersihkan_isbn(isbn_final);
        if (isbn_valid && !ean13_valid(*isbn_valid)) isbn_valid.reset();
        {
            auto upd = siapkan(db,
                "UPDATE cache_metadata SET penerbit=?, isbn=COALESCE(?,isbn),"
                " sumber='rag_manual', status='dipromosikan' WHERE judul_penulis_key=?");
            bind_teks(upd.get(), 1, penerbit_final);
            bind_opsional(upd.get(), 2, isbn_valid);
            bind_teks(upd.get(), 3, key);
            if (sqlite3_step(upd.get()) != SQLITE_DONE) gagal(db, "update cache");
        }

        jalankan(db, "COMMIT");
        std::cout << "Dipromosikan: '" << judul << "' -> rag_manual (penerbit: "
                  << penerbit_final << ")\n";
    } catch (...) {
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
}

// ---------------------------------------------------------------------------------------
// Promosi otomatis
// ---------------------------------------------------------------------------------------
namespace {

struct Calon {
    std::string key, judul, penulis, penerbit, status;
    std::optional<std::string> isbn;  // sudah dibersihkan
};

// Data buku yang SUDAH ada di rag_manual (pemenang). Dipakai untuk mendeteksi duplikat dan
// untuk menyamakan baris cache yang kalah.
struct Menang {
    std::string penerbit;
    std::optional<std::string> isbn;  // sudah dibersihkan
};

std::string teks_atau_kosong(sqlite3_stmt* st, int i) {
    return sqlite3_column_type(st, i) == SQLITE_NULL ? std::string() : kolom_teks(st, i);
}

// Kunci pembanding judul+penulis, tanpa peduli huruf besar-kecil (sama dengan Penyimpan).
std::string kunci_judul(const std::string& judul, const std::string& penulis) {
    return huruf_kecil(judul) + '\x1f' + huruf_kecil(penulis);
}

}  // namespace

void meta_promosi_otomatis(sqlite3* db, bool cek, bool rinci,
                           const std::function<void()>& sebelum_tulis) {
    // 1. Calon: entri cache baru/ditinjau, urut dari yang paling dulu masuk.
    std::vector<Calon> calon;
    {
        auto st = siapkan(db,
            "SELECT judul_penulis_key, judul_asli, penulis_asli, penerbit, isbn,"
            " COALESCE(status,'baru')"
            " FROM cache_metadata WHERE COALESCE(status,'baru') IN ('baru','ditinjau')"
            " ORDER BY COALESCE(waktu_masuk,''), judul_penulis_key");
        while (sqlite3_step(st.get()) == SQLITE_ROW) {
            Calon c;
            c.key = kolom_teks(st.get(), 0);
            c.judul = trim(teks_atau_kosong(st.get(), 1));
            c.penulis = trim(teks_atau_kosong(st.get(), 2));
            c.penerbit = trim(teks_atau_kosong(st.get(), 3));
            if (sqlite3_column_type(st.get(), 4) != SQLITE_NULL)
                c.isbn = bersihkan_isbn(kolom_teks(st.get(), 4));
            c.status = kolom_teks(st.get(), 5);
            calon.push_back(std::move(c));
        }
    }

    // 2. Yang sudah ada di rag_manual (ISBN dibersihkan dengan fungsi yang sama). Kalau ada
    //    kembar di rag_manual, yang id-nya paling kecil dianggap pemenang.
    std::unordered_map<std::string, Menang> menang_judul, menang_isbn;
    {
        auto st = siapkan(db,
            "SELECT isbn, LOWER(COALESCE(judul,'')), LOWER(COALESCE(penulis,'')),"
            " COALESCE(penerbit,'') FROM rag_manual ORDER BY id");
        while (sqlite3_step(st.get()) == SQLITE_ROW) {
            Menang m;
            m.penerbit = trim(kolom_teks(st.get(), 3));
            if (sqlite3_column_type(st.get(), 0) != SQLITE_NULL)
                m.isbn = bersihkan_isbn(kolom_teks(st.get(), 0));
            menang_judul.emplace(kunci_judul(kolom_teks(st.get(), 1), kolom_teks(st.get(), 2)), m);
            if (m.isbn) menang_isbn.emplace(*m.isbn, m);
        }
    }

    std::optional<Penyimpan> p;
    if (!cek) p.emplace(db);

    int naik = 0, menunggu = 0, dilewati = 0, disamakan = 0, gagal_n = 0;
    bool sudah_siap = false; 
    auto siap_tulis = [&] {   
        if (!sudah_siap) {
            if (sebelum_tulis) sebelum_tulis();
            sudah_siap = true;
        }
    };
    std::map<std::string, int> alasan_menunggu;

    std::cout << (cek ? "Promosi otomatis (CEK SAJA, tidak ada yang ditulis)\n"
                      : "Promosi otomatis\n");

    for (const Calon& c : calon) {
        std::string tunggu;
        if (c.judul.empty()) tunggu = "judul_asli kosong";
        else if (c.penulis.empty()) tunggu = "penulis kosong";
        else if (c.penerbit.empty()) tunggu = "penerbit kosong";
        else if (!c.isbn) tunggu = "ISBN kosong";
        else if (!isbn13_valid(*c.isbn)) tunggu = "ISBN bukan ISBN-13 yang valid";
        if (!tunggu.empty()) {
            ++menunggu;
            ++alasan_menunggu[tunggu];
            if (rinci) std::cout << "  [menunggu] " << c.key << ": " << tunggu << "\n";
            continue;
        }

        // 3b. Tidak boleh menimpa: yang sudah ada di rag_manual (atau baru naik di putaran
        //     ini) tidak naik.
        const std::string kj = kunci_judul(c.judul, c.penulis);
        const Menang* pemenang = nullptr;
        std::string duplikat;
        if (auto it = menang_judul.find(kj); it != menang_judul.end()) {
            pemenang = &it->second;
            duplikat = "judul+penulis sudah ada di rag_manual";
        } else if (auto it2 = menang_isbn.find(*c.isbn); it2 != menang_isbn.end()) {
            pemenang = &it2->second;
            duplikat = "ISBN sudah ada di rag_manual";
        }
        if (pemenang) {
            ++dilewati;
            if (rinci) std::cout << "  [lewati]   " << c.key << ": " << duplikat << "\n";

            // Samakan baris cache yang kalah dengan pemenangnya. Lookup (cache dulu, baru
            // rag_manual) akan menampilkan baris cache ini; tanpa penyamaan, data yang kalah
            // tetap menang. Hanya status 'baru': 'ditinjau' sedang dipegang admin (mungkin
            // berisi koreksi yang belum dipromosikan). Nilai kosong di pemenang tidak dipakai.
            if (c.status == "baru") {
                const std::string penerbit_baru =
                    pemenang->penerbit.empty() ? c.penerbit : pemenang->penerbit;
                const std::optional<std::string> isbn_baru =
                    pemenang->isbn ? pemenang->isbn : c.isbn;
                if (penerbit_baru != c.penerbit || isbn_baru != c.isbn) {
                    if (!cek) {
                        siap_tulis();
                        try {
                            auto upd = siapkan(db,
                                "UPDATE cache_metadata SET penerbit=?, isbn=?, sumber='rag_manual'"
                                " WHERE judul_penulis_key=? AND COALESCE(status,'baru')='baru'");
                            bind_teks(upd.get(), 1, penerbit_baru);
                            bind_opsional(upd.get(), 2, isbn_baru);
                            bind_teks(upd.get(), 3, c.key);
                            if (sqlite3_step(upd.get()) != SQLITE_DONE) gagal(db, "samakan cache");
                        } catch (const std::exception& e) {
                            ++gagal_n;
                            std::cerr << "  [GAGAL]    " << c.key << ": " << e.what() << "\n";
                            continue;
                        }
                    }
                    ++disamakan;
                    std::cout << "  [samakan]  " << c.key << ":";
                    if (penerbit_baru != c.penerbit)
                        std::cout << " penerbit '" << c.penerbit << "' -> '" << penerbit_baru << "'";
                    if (isbn_baru != c.isbn)
                        std::cout << " ISBN " << c.isbn.value_or("-") << " -> " << isbn_baru.value_or("-");
                    std::cout << " (mengikuti rag_manual)\n";
                }
            }
            continue;
        }

        // 3c. Naikkan. Satu transaksi per baris: satu baris gagal tidak membatalkan yang lain.
        if (!cek) {
            siap_tulis();
            try {
                jalankan(db, "BEGIN");
                const Hasil h = p->simpan(c.judul, c.penulis, c.penerbit, *c.isbn,
                                          "promosi_otomatis", false);
                if (h != Hasil::Baru)  // jaring pengaman: batalkan kalau sampai menimpa
                    throw std::runtime_error("rag_manual ternyata sudah punya buku ini, tidak ditimpa");
                {
                    auto upd = siapkan(db,
                        "UPDATE cache_metadata SET penerbit=?, isbn=?,"
                        " sumber='rag_manual', status='dipromosikan' WHERE judul_penulis_key=?");
                    bind_teks(upd.get(), 1, c.penerbit);
                    bind_teks(upd.get(), 2, *c.isbn);
                    bind_teks(upd.get(), 3, c.key);
                    if (sqlite3_step(upd.get()) != SQLITE_DONE) gagal(db, "update cache");
                }
                jalankan(db, "COMMIT");
            } catch (const std::exception& e) {
                sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
                ++gagal_n;
                std::cerr << "  [GAGAL]    " << c.key << ": " << e.what() << "\n";
                continue;
            }
        }
        Menang m;
        m.penerbit = c.penerbit;
        m.isbn = c.isbn;
        menang_judul.emplace(kj, m);
        menang_isbn.emplace(*c.isbn, m);
        ++naik;
        std::cout << "  [naik]     " << c.key << ": '" << c.judul << "' -> rag_manual (penerbit: "
                  << c.penerbit << ", ISBN: " << *c.isbn << ")\n";
    }

    std::cout << "Hasil: " << naik << (cek ? " akan naik, " : " naik, ") << menunggu
              << " menunggu, " << dilewati << " dilewati (sudah ada), " << gagal_n << " gagal.\n";
    for (const auto& [alasan, n] : alasan_menunggu)
        std::cout << "  menunggu - " << alasan << ": " << n << "\n";
    if (disamakan > 0)
        std::cout << "  disamakan dengan rag_manual: " << disamakan << (cek ? " (akan)" : "") << "\n";
    if (!rinci && (menunggu > 0 || dilewati > 0))
        std::cout << "  (tambahkan --rinci untuk melihat baris yang menunggu/dilewati)\n";
    if (gagal_n > 0)  // exit code tidak 0 supaya kegagalan terlihat oleh penjadwal/notebook
        throw std::runtime_error(std::to_string(gagal_n) + " baris gagal dipromosikan (lihat [GAGAL] di atas)");
}
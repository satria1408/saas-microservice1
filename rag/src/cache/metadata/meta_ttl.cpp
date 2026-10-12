#include "meta_ttl.h"

#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace {

// Satu baris per tabel yang dikenai TTL.
struct Tabel {
    const char* nama;           // nama tabel
    const char* kolom_kelas;    // kolom yang menentukan kelas baris ("" = satu kelas)
    const char* kelas;          // ekspresi SQL kelas baris (untuk laporan)
    const char* kunci;          // ekspresi SQL: sasaran aturan untuk baris ini
    const char* trigger_ttl;    // trigger pembuangan
    const char* trigger_waktu;  // trigger pengisi waktu_masuk
    const char* indeks_nama;
    const char* indeks_kolom;
};

const Tabel TABEL[] = {
    {"cache_metadata", "status", "COALESCE(status,'baru')",
     "'metadata.' || COALESCE(cache_metadata.status,'baru')",
     "cache_meta_ttl", "cache_meta_isi_waktu", "idx_cache_meta_ttl", "status, waktu_masuk"},
    {"cache_scan", "", "'(semua)'",
     "'scan'",
     "scan_ttl", "scan_isi_waktu", "idx_scan_waktu", "waktu_masuk"},
    {"katalog", "status_konfirmasi", "COALESCE(status_konfirmasi,'otomatis')",
     "'katalog.' || COALESCE(katalog.status_konfirmasi,'otomatis')",
     "katalog_ttl", "katalog_isi_waktu", "idx_katalog_ttl", "status_konfirmasi, waktu_masuk"},
};

const std::string DARI_TABEL = "SELECT sasaran, hari FROM ttl_aturan";

bool tabel_ada(sqlite3* db, const std::string& nama) {
    auto st = siapkan(db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?");
    bind_teks(st.get(), 1, nama);
    return sqlite3_step(st.get()) == SQLITE_ROW;
}

bool kolom_ada(sqlite3* db, const std::string& tabel, const std::string& kolom) {
    auto st = siapkan(db, ("PRAGMA table_info(" + tabel + ")").c_str());
    while (sqlite3_step(st.get()) == SQLITE_ROW)
        if (kolom_teks(st.get(), 1) == kolom) return true;
    return false;
}

// Tabel siap dikenai TTL: ada, punya waktu_masuk, dan punya kolom kelasnya.
bool siap(sqlite3* db, const Tabel& t) {
    if (!tabel_ada(db, t.nama) || !kolom_ada(db, t.nama, "waktu_masuk")) return false;
    const std::string kk = t.kolom_kelas;
    return kk.empty() || kolom_ada(db, t.nama, kk);
}

std::map<std::string, int> peta_dari(const AturanTtl& a) {
    return {{"metadata.baru", a.meta_baru},
            {"metadata.ditolak", a.meta_ditolak},
            {"metadata.dipromosikan", a.meta_dipromosikan},
            {"metadata.ditinjau", 0},
            {"scan", a.scan},
            {"katalog.otomatis", a.katalog_otomatis},
            {"katalog.terkonfirmasi", 0}};
}

// Aturan yang berlaku: dari tabel ttl_aturan kalau sudah dipasang, kalau belum nilai bawaan.
std::map<std::string, int> baca_aturan(sqlite3* db, bool& dari_tabel) {
    auto peta = peta_dari(AturanTtl{});
    dari_tabel = tabel_ada(db, "ttl_aturan");
    if (!dari_tabel) return peta;
    auto st = siapkan(db, DARI_TABEL.c_str());
    while (sqlite3_step(st.get()) == SQLITE_ROW) {
        const std::string k = kolom_teks(st.get(), 0);
        if (peta.count(k)) peta[k] = static_cast<int>(sqlite3_column_int64(st.get(), 1));
    }
    return peta;
}

// Aturan sebagai subquery (sasaran, hari) tanpa perlu tabel: dipakai pembersihan manual.
std::string sumber_inline(const std::map<std::string, int>& peta) {
    std::string s;
    for (const auto& [k, v] : peta) {
        s += s.empty() ? "SELECT '" : " UNION ALL SELECT '";
        s += k + "' AS sasaran, " + std::to_string(v) + " AS hari";
    }
    return s;
}

// SATU-SATUNYA tempat kondisi "baris ini kedaluwarsa" ditulis. Dipakai trigger, penghitungan,
// dan penghapusan, jadi tidak mungkin berbeda.
std::string kondisi(const Tabel& t, const std::string& sumber_aturan) {
    return std::string(t.nama) + ".waktu_masuk IS NOT NULL AND EXISTS (SELECT 1 FROM (" +
           sumber_aturan + ") a WHERE a.hari > 0 AND a.sasaran = " + t.kunci + " AND " + t.nama +
           ".waktu_masuk < datetime('now', '-' || a.hari || ' days'))";
}

std::map<std::string, long long> hitung_kedaluwarsa(sqlite3* db, const Tabel& t,
                                                    const std::string& sumber) {
    std::map<std::string, long long> hasil;
    auto st = siapkan(db, (std::string("SELECT ") + t.kelas + ", COUNT(*) FROM " + t.nama +
                           " WHERE " + kondisi(t, sumber) + " GROUP BY 1").c_str());
    while (sqlite3_step(st.get()) == SQLITE_ROW)
        hasil[kolom_teks(st.get(), 0)] = sqlite3_column_int64(st.get(), 1);
    return hasil;
}

long long jumlah(const std::map<std::string, long long>& m) {
    long long n = 0;
    for (const auto& [k, v] : m) n += v;
    return n;
}

std::string rincian(const std::map<std::string, long long>& m) {
    std::string s;
    for (const auto& [k, v] : m) s += (s.empty() ? "" : ", ") + k + ": " + std::to_string(v);
    return s;
}

}  // namespace

void ttl_pasang(sqlite3* db, const AturanTtl& a) {
    const auto peta = peta_dari(a);
    bool ada_aktif = false;
    for (const auto& [k, v] : peta) {
        if (v < 0 || v > 36500) throw std::runtime_error("hari harus 0..36500 (0 = tidak dibuang)");
        if (v > 0) ada_aktif = true;
    }
    if (!ada_aktif) throw std::runtime_error("semua nilai 0: tidak ada yang dipasang (pakai: cache ttl lepas)");

    std::string terpasang, dilewati;
    jalankan(db, "BEGIN");
    try {
        jalankan(db,
            "CREATE TABLE IF NOT EXISTS ttl_aturan ("
            " sasaran TEXT PRIMARY KEY,"
            " hari INTEGER NOT NULL CHECK (hari >= 0 AND hari <= 36500))");
        {
            auto ins = siapkan(db, "INSERT OR REPLACE INTO ttl_aturan (sasaran, hari) VALUES (?, ?)");
            for (const auto& [k, v] : peta) {
                bind_teks(ins.get(), 1, k);
                sqlite3_bind_int(ins.get(), 2, v);
                if (sqlite3_step(ins.get()) != SQLITE_DONE) gagal(db, "simpan aturan ttl");
                sqlite3_reset(ins.get());
            }
        }

        for (const Tabel& t : TABEL) {
            if (!siap(db, t)) { dilewati += std::string(dilewati.empty() ? "" : ", ") + t.nama; continue; }
            terpasang += std::string(terpasang.empty() ? "" : ", ") + t.nama;
            const std::string tabel = t.nama;

            // Baris lama tanpa waktu_masuk tidak pernah kedaluwarsa. Mulai dihitung dari sekarang,
            // jadi tidak ada yang langsung terhapus.
            jalankan(db, ("UPDATE " + tabel + " SET waktu_masuk=datetime('now') WHERE waktu_masuk IS NULL").c_str());
            jalankan(db, (std::string("CREATE INDEX IF NOT EXISTS ") + t.indeks_nama + " ON " + tabel +
                          "(" + t.indeks_kolom + ")").c_str());

            // Penulis yang tidak mengisi waktu_masuk (notebook) tetap kebagian.
            jalankan(db, (std::string("CREATE TRIGGER IF NOT EXISTS ") + t.trigger_waktu +
                          " AFTER INSERT ON " + tabel + " WHEN NEW.waktu_masuk IS NULL BEGIN UPDATE " +
                          tabel + " SET waktu_masuk=datetime('now') WHERE rowid=NEW.rowid; END").c_str());

            jalankan(db, (std::string("DROP TRIGGER IF EXISTS ") + t.trigger_ttl).c_str());
            jalankan(db, (std::string("CREATE TRIGGER ") + t.trigger_ttl + " AFTER INSERT ON " + tabel +
                          " BEGIN DELETE FROM " + tabel + " WHERE " + kondisi(t, DARI_TABEL) +
                          "; END").c_str());
        }
        jalankan(db, "COMMIT");
    } catch (...) {
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }

    std::cout << "TTL dipasang (hari; 0 = tidak dibuang):\n"
              << "  cache_metadata : baru=" << a.meta_baru << ", ditolak=" << a.meta_ditolak
              << ", dipromosikan=" << a.meta_dipromosikan << " ('ditinjau' tidak pernah dibuang)\n"
              << "  cache_scan     : " << a.scan << "\n"
              << "  katalog        : otomatis=" << a.katalog_otomatis
              << " ('terkonfirmasi' tidak pernah dibuang)\n"
              << "Umur dihitung dari waktu_masuk (pertama masuk). Baris tanpa waktu_masuk mulai dihitung dari sekarang.\n"
              << "Pembuangan otomatis jalan tiap ada INSERT ke tabelnya; 'rag cache bersihkan' menjalankannya sekarang.\n";
    if (!dilewati.empty())
        std::cout << "Dilewati (tabel/kolom belum ada, jalankan rag migrasi): " << dilewati << "\n";
}

void ttl_lepas(sqlite3* db) {
    for (const Tabel& t : TABEL)
        jalankan(db, (std::string("DROP TRIGGER IF EXISTS ") + t.trigger_ttl).c_str());
    std::cout << "Trigger TTL dicabut dari ketiga tabel. Tidak ada lagi pembuangan otomatis.\n"
              << "(Tabel ttl_aturan dan pengisi waktu_masuk dibiarkan, tidak berbahaya. Jangan hapus tabel ttl_aturan "
                 "selagi trigger masih terpasang.)\n";
}

void ttl_status(sqlite3* db) {
    bool dari_tabel = false;
    const auto peta = baca_aturan(db, dari_tabel);

    std::cout << "Trigger pembuangan otomatis:\n";
    for (const Tabel& t : TABEL) {
        std::string keadaan;
        if (!siap(db, t)) keadaan = "tabel/kolom belum ada";
        else {
            auto st = siapkan(db, "SELECT 1 FROM sqlite_master WHERE type='trigger' AND name=?");
            bind_teks(st.get(), 1, t.trigger_ttl);
            keadaan = sqlite3_step(st.get()) == SQLITE_ROW ? "AKTIF" : "tidak terpasang";
        }
        std::cout << "  " << t.nama << ": " << keadaan << "\n";
    }

    std::cout << "\nAturan (hari; 0 = tidak dibuang) - sumber: "
              << (dari_tabel ? "tabel ttl_aturan" : "nilai bawaan (belum dipasang: rag cache ttl pasang)") << "\n";
    for (const auto& [k, v] : peta) std::cout << "  " << k << " = " << v << "\n";

    const std::string sumber = sumber_inline(peta);
    std::cout << "\nIsi tabel (kedaluwarsa = sudah melewati aturan, menunggu dibuang):\n";
    for (const Tabel& t : TABEL) {
        if (!siap(db, t)) continue;
        std::cout << "  " << t.nama << "\n";
        auto st = siapkan(db, (std::string("SELECT ") + t.kelas + ", COUNT(*), SUM(waktu_masuk IS NULL),"
                               " SUM(CASE WHEN " + kondisi(t, sumber) + " THEN 1 ELSE 0 END)"
                               " FROM " + t.nama + " GROUP BY 1 ORDER BY 1").c_str());
        int rc;
        while ((rc = sqlite3_step(st.get())) == SQLITE_ROW)
            std::cout << "    " << kolom_teks(st.get(), 0) << ": " << sqlite3_column_int64(st.get(), 1)
                      << " baris, " << sqlite3_column_int64(st.get(), 3) << " kedaluwarsa, "
                      << sqlite3_column_int64(st.get(), 2) << " tanpa waktu_masuk\n";
        if (rc != SQLITE_DONE) gagal(db, "membaca status ttl");
    }
}

void ttl_bersihkan(sqlite3* db, int hari_override, bool yes,
                   const std::function<void()>& sebelum_tulis) {
    if (hari_override < 0) throw std::runtime_error("--hari tidak boleh negatif");

    bool dari_tabel = false;
    auto peta = baca_aturan(db, dari_tabel);
    if (hari_override > 0) {
        peta["metadata.baru"] = hari_override;
        peta["scan"] = hari_override;
        peta["katalog.otomatis"] = hari_override;
    }
    const std::string sumber = sumber_inline(peta);

    std::cout << "Aturan TTL (hari): sumber "
              << (hari_override > 0 ? "--hari (untuk baru/scan/katalog otomatis) + " : "")
              << (dari_tabel ? "tabel ttl_aturan" : "nilai bawaan") << "\n";
    for (const auto& [k, v] : peta) std::cout << "  " << k << " = " << v << "\n";

    std::map<std::string, std::map<std::string, long long>> kedaluwarsa;  // tabel -> kelas -> jumlah
    long long total = 0;
    for (const Tabel& t : TABEL) {
        if (!siap(db, t)) {
            std::cout << t.nama << " : dilewati (tabel/kolom belum ada, jalankan rag migrasi)\n";
            continue;
        }
        auto m = hitung_kedaluwarsa(db, t, sumber);
        total += jumlah(m);
        std::cout << t.nama << " : " << jumlah(m) << " kedaluwarsa"
                  << (m.empty() ? "" : " (" + rincian(m) + ")") << "\n";
        kedaluwarsa[t.nama] = std::move(m);
    }

    if (total == 0) {
        std::cout << "Tidak ada yang kedaluwarsa. Tidak ada yang dihapus.\n";
        return;
    }

    if (!yes) {
        std::cout << "Hapus " << total << " baris? (y/N) ";
        std::string jawab;
        std::getline(std::cin, jawab);
        if (jawab != "y" && jawab != "Y") {
            std::cout << "Dibatalkan.\n";
            return;
        }
    }

    if (sebelum_tulis) sebelum_tulis();

    jalankan(db, "BEGIN");  // satu transaksi untuk ketiga tabel: semua terhapus atau tidak sama sekali
    try {
        for (const Tabel& t : TABEL) {
            if (!kedaluwarsa.count(t.nama)) continue;
            jalankan(db, (std::string("DELETE FROM ") + t.nama + " WHERE " + kondisi(t, sumber)).c_str());
            std::cout << "[OK] " << t.nama << ": " << sqlite3_changes(db) << " baris dihapus\n";
        }
        jalankan(db, "COMMIT");
    } catch (...) {
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
}
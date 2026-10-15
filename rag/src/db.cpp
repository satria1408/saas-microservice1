#include "db.h"

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <stdexcept>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <system_error>
#include <vector>

#include "skema/skema_inti.h"
#include "util.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

[[noreturn]] void gagal(sqlite3* db, const std::string& konteks) {
    throw std::runtime_error(konteks + ": " + sqlite3_errmsg(db));
}

DbPtr buka_db(const std::string& path, ModeBuka mode) {
    int flags = SQLITE_OPEN_READWRITE;
    if (mode == ModeBuka::HanyaBaca) flags = SQLITE_OPEN_READONLY;
    else if (mode == ModeBuka::Buat) flags |= SQLITE_OPEN_CREATE;

    sqlite3* mentah = nullptr;
    int rc = sqlite3_open_v2(path.c_str(), &mentah, flags, nullptr);
    DbPtr db(mentah);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Tidak bisa membuka '" + path +
                                 "': " + (mentah ? sqlite3_errmsg(mentah) : "out of memory"));
    }
    sqlite3_busy_timeout(mentah, 5000);

    {
        auto st = siapkan(mentah, "PRAGMA journal_mode");
        const bool wal = sqlite3_step(st.get()) == SQLITE_ROW && huruf_kecil(kolom_teks(st.get(), 0)) == "wal";
        st.reset();
        if (wal) jalankan(mentah, "PRAGMA synchronous=NORMAL");
    }
    return db;
}


StmtPtr siapkan(sqlite3* db, const char* sql) {
    sqlite3_stmt* mentah = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &mentah, nullptr) != SQLITE_OK) gagal(db, "prepare");
    return StmtPtr(mentah);
}

void bind_teks(sqlite3_stmt* st, int idx, const std::string& s) {
    sqlite3_bind_text(st, idx, s.c_str(), static_cast<int>(s.size()), SQLITE_TRANSIENT);
}

void bind_opsional(sqlite3_stmt* st, int idx, const std::optional<std::string>& s) {
    if (s) bind_teks(st, idx, *s);
    else sqlite3_bind_null(st, idx);
}

std::string kolom_teks(sqlite3_stmt* st, int i) {
    const unsigned char* p = sqlite3_column_text(st, i);
    return p ? reinterpret_cast<const char*>(p) : "-";
}

void jalankan(sqlite3* db, const char* sql) {
    char* pesan = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &pesan) != SQLITE_OK) {
        std::string e = pesan ? pesan : "?";
        sqlite3_free(pesan);
        throw std::runtime_error(std::string(sql) + ": " + e);
    }
}

bool punya_tabel(sqlite3* db, const std::string& tabel) {
    auto st = siapkan(db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?");
    bind_teks(st.get(), 1, tabel);
    return sqlite3_step(st.get()) == SQLITE_ROW;
}

bool punya_kolom(sqlite3* db, const std::string& tabel, const std::string& kolom) {
    auto st = siapkan(db, ("PRAGMA table_info(" + tabel + ")").c_str());
    while (sqlite3_step(st.get()) == SQLITE_ROW)
        if (kolom_teks(st.get(), 1) == kolom) return true;
    return false;
}

void pastikan_skema(sqlite3* db) { pastikan_skema_inti(db); }

// Nama harus persis "<file>.bak-YYYYMMDD-HHMMSS" atau "<file>.bak-YYYYMMDD-HHMMSS-NN"
// (NN = dua digit, 02..99, untuk backup yang jatuh pada detik yang sama), supaya file lain
// tidak ikut terhapus.
static bool nama_backup_valid(const std::string& nama, const std::string& awalan) {
    const size_t dasar = awalan.size() + 15;
    if (nama.size() != dasar && nama.size() != dasar + 3) return false;
    if (nama.compare(0, awalan.size(), awalan) != 0) return false;
    for (size_t i = 0; i < 15; ++i) {
        const char c = nama[awalan.size() + i];
        if (i == 8) { if (c != '-') return false; }
        else if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    if (nama.size() == dasar + 3) {
        if (nama[dasar] != '-') return false;
        if (!std::isdigit(static_cast<unsigned char>(nama[dasar + 1])) ||
            !std::isdigit(static_cast<unsigned char>(nama[dasar + 2]))) return false;
    }
    return true;
}

namespace {

// Salin isi `sumber` ke file baru `sementara` lewat backup API, jadikan satu file mandiri
// (journal_mode=DELETE, tanpa -wal/-shm), lalu periksa. Melempar kalau ada yang salah.
void salin_dengan_backup_api(sqlite3* sumber, const std::string& sementara) {
    sqlite3* mentah = nullptr;
    const int rc_buka = sqlite3_open_v2(sementara.c_str(), &mentah,
                                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    DbPtr tujuan(mentah);
    if (rc_buka != SQLITE_OK)
        throw std::runtime_error("backup: tidak bisa membuat '" + sementara + "': " +
                                 (mentah ? sqlite3_errmsg(mentah) : "out of memory"));

    sqlite3_backup* b = sqlite3_backup_init(tujuan.get(), "main", sumber, "main");
    if (!b) gagal(tujuan.get(), "backup init");

    // step(-1) menyalin semuanya dalam satu snapshot yang konsisten. BUSY/LOCKED berarti ada
    // penulis lain yang sedang commit: tunggu dan coba lagi (maks 10 detik).
    const auto batas = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    int rc;
    while ((rc = sqlite3_backup_step(b, -1)) == SQLITE_BUSY || rc == SQLITE_LOCKED) {
        if (std::chrono::steady_clock::now() > batas) break;
        sqlite3_sleep(100);
    }
    sqlite3_backup_finish(b);
    if (rc != SQLITE_DONE) {
        if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED)
            throw std::runtime_error("backup: database sedang dikunci penulis lain (menunggu 10 detik)");
        throw std::runtime_error(std::string("backup: gagal menyalin: ") + sqlite3_errstr(rc));
    }

    // Kalau sumber WAL, header salinan ikut bertanda WAL. Jadikan file tunggal yang mandiri.
    jalankan(tujuan.get(), "PRAGMA journal_mode=DELETE");

    auto cek = siapkan(tujuan.get(), "PRAGMA quick_check");
    std::string hasil;
    while (sqlite3_step(cek.get()) == SQLITE_ROW) {
        if (!hasil.empty()) hasil += "; ";
        hasil += kolom_teks(cek.get(), 0);
        if (hasil.size() > 300) break;
    }
    if (hasil != "ok") throw std::runtime_error("backup tidak lolos quick_check: " + hasil);
}

}  // namespace

void buat_backup(sqlite3* db, const std::string& path, int simpan) {
    namespace fs = std::filesystem;
    if (simpan < 1) throw std::runtime_error("jumlah backup yang disimpan minimal 1");
    std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y%m%d-%H%M%S", std::localtime(&t));
    const std::string dasar = path + ".bak-" + buf;
    // Jangan pernah menimpa backup yang ada: kalau detik ini sudah punya backup (perintah tulis
    // lain dalam detik yang sama), tambahkan akhiran -02, -03, ... (dua digit supaya urutan
    // string tetap kronologis). Nomor dipilih SETELAH yang tertinggi yang ada, bukan mengisi
    // celah: kalau nama dasar sudah dibuang rotasi, memakainya lagi akan membuat backup
    // terbaru terurut paling lama dan langsung dibuang.
    int tertinggi = fs::exists(dasar) ? 1 : 0;
    for (int n = 2; n <= 99; ++n) {
        char akhiran[8];
        std::snprintf(akhiran, sizeof akhiran, "-%02d", n);
        if (fs::exists(dasar + akhiran)) tertinggi = n;
    }
    std::string tujuan = dasar;
    if (tertinggi > 0) {
        if (tertinggi >= 99) throw std::runtime_error("terlalu banyak backup dalam satu detik: " + dasar);
        char akhiran[8];
        std::snprintf(akhiran, sizeof akhiran, "-%02d", tertinggi + 1);
        tujuan = dasar + akhiran;
    }

    // Tulis ke .tmp (namanya tidak cocok pola rotasi, jadi tidak pernah ikut dihitung/dibuang),
    // baru di-rename setelah terbukti sehat. Backup setengah jadi tidak pernah punya nama akhir.
    const std::string sementara = tujuan + ".tmp";
    std::error_code ec;
    fs::remove(sementara, ec);
    try {
        salin_dengan_backup_api(db, sementara);
        fs::rename(sementara, tujuan);
    } catch (...) {
        for (const char* sfx : {"", "-wal", "-shm", "-journal"}) fs::remove(sementara + sfx, ec);
        throw;
    }
    std::cout << "[backup] " << tujuan << "\n";

    // Rotasi. Gagal merotasi (mis. file dikunci OneDrive) tidak boleh membatalkan perintah.
    try {
        const fs::path p(path);
        const fs::path folder = p.has_parent_path() ? p.parent_path() : fs::path(".");
        const std::string awalan = p.filename().string() + ".bak-";
        std::vector<std::string> daftar;
        for (const auto& e : fs::directory_iterator(folder)) {
            if (!e.is_regular_file()) continue;
            const std::string nama = e.path().filename().string();
            if (nama_backup_valid(nama, awalan)) daftar.push_back(nama);
        }
        std::sort(daftar.begin(), daftar.end());  // format waktu tetap -> urut kronologis
        int dibuang = 0;
        while (static_cast<int>(daftar.size()) > simpan) {
            fs::remove(folder / daftar.front());
            daftar.erase(daftar.begin());
            ++dibuang;
        }
        if (dibuang > 0)
            std::cout << "[backup] " << dibuang << " backup lama dibuang (simpan " << simpan
                      << " terakhir)\n";
    } catch (const fs::filesystem_error& e) {
        std::cerr << "[WARN] rotasi backup gagal: " << e.what() << "\n";
    }
}

// ---------------------------------------------------------------------------------------
// WAL: pengaman sebelum dipasang
// ---------------------------------------------------------------------------------------
std::string lokasi_berisiko(const std::string& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path mutlak = fs::absolute(path, ec);
    if (ec) mutlak = path;

    auto normal = [](std::string s) {
        for (char& ch : s) ch = (ch == '\\') ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return s;
    };
    const std::string p = normal(mutlak.string());

    // Drive jaringan / UNC: WAL butuh shared memory (-shm), tidak jalan lewat network filesystem.
    // Dicek pada path mentah juga, sebelum fs::absolute (di Windows UNC tetap utuh, tapi jangan
    // bergantung pada itu).
    if (p.rfind("//", 0) == 0 || normal(path).rfind("//", 0) == 0)
        return "path UNC/jaringan (WAL tidak jalan di network filesystem)";
#ifdef _WIN32
    {
        const std::string akar = mutlak.root_path().string();
        if (!akar.empty() && GetDriveTypeA(akar.c_str()) == DRIVE_REMOTE)
            return "drive jaringan (WAL tidak jalan di network filesystem)";
    }
#endif

    // Folder sinkronisasi: mereka menyalin/mengunci file di belakang layar, dan SQLite tidak
    // aman di sana. Dengan WAL ada tiga file (.db, -wal, -shm) yang harus selalu konsisten.
    for (const char* nama : {"OneDrive", "OneDriveConsumer", "OneDriveCommercial"}) {
        if (const char* e = std::getenv(nama)) {
            std::string akar = normal(e);
            while (!akar.empty() && akar.back() == '/') akar.pop_back();
            if (!akar.empty() && (p == akar || p.rfind(akar + "/", 0) == 0))
                return "di dalam folder OneDrive";
        }
    }
    for (const char* kunci : {"/onedrive", "/dropbox", "/google drive", "/googledrive", "/icloud"})
        if (p.find(kunci) != std::string::npos)
            return std::string("path mengandung folder sinkronisasi (") + (kunci + 1) + ")";
    return "";
}

void aktifkan_wal(sqlite3* db, const std::string& path, bool paksa) {
    if (!paksa) {
        const std::string alasan = lokasi_berisiko(path);
        if (!alasan.empty())
            throw std::runtime_error("WAL tidak dipasang: " + alasan +
                                     ". Pindahkan DB ke disk lokal di luar folder sinkronisasi.");
    }

    auto st = siapkan(db, "PRAGMA journal_mode=WAL");
    if (sqlite3_step(st.get()) != SQLITE_ROW) gagal(db, "journal_mode");
    const std::string mode = huruf_kecil(kolom_teks(st.get(), 0));
    st.reset();
    if (mode != "wal")
        throw std::runtime_error("SQLite menolak WAL (mode tetap '" + mode + "')");

    jalankan(db, "PRAGMA synchronous=NORMAL");
}
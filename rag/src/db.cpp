#include "db.h"

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <stdexcept>

#include <algorithm>
#include <cctype>
#include <vector>

[[noreturn]] void gagal(sqlite3* db, const std::string& konteks) {
    throw std::runtime_error(konteks + ": " + sqlite3_errmsg(db));
}

DbPtr buka_db(const std::string& path) {
    sqlite3* mentah = nullptr;
    int rc = sqlite3_open_v2(path.c_str(), &mentah, SQLITE_OPEN_READWRITE, nullptr);
    DbPtr db(mentah);  // serahkan ke unique_ptr DULU, walau open gagal
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Tidak bisa membuka '" + path +
                                 "': " + (mentah ? sqlite3_errmsg(mentah) : "out of memory"));
    }
    sqlite3_busy_timeout(mentah, 5000); 
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

void pastikan_skema(sqlite3* db) {
    jalankan(db,
        "CREATE TABLE IF NOT EXISTS rag_manual ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " judul TEXT, penulis TEXT, penerbit TEXT,"
        " sumber TEXT DEFAULT 'input_manual',"
        " waktu_masuk TEXT DEFAULT CURRENT_TIMESTAMP)");

    auto st = siapkan(db, "PRAGMA table_info(rag_manual)");
    bool ada_isbn = false;
    while (sqlite3_step(st.get()) == SQLITE_ROW)
        if (kolom_teks(st.get(), 1) == "isbn") ada_isbn = true;
    st.reset();
    if (!ada_isbn) jalankan(db, "ALTER TABLE rag_manual ADD COLUMN isbn TEXT");
}

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

void buat_backup(const std::string& path, int simpan) {
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
    fs::copy_file(path, tujuan, fs::copy_options::none);
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
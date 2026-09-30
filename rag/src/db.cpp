#include "db.h"

#include <ctime>
#include <filesystem>
#include <iostream>
#include <stdexcept>

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

void buat_backup(const std::string& path) {
    namespace fs = std::filesystem;
    std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y%m%d-%H%M%S", std::localtime(&t));
    std::string tujuan = path + ".bak-" + buf;
    fs::copy_file(path, tujuan, fs::copy_options::overwrite_existing);
    std::cout << "[backup] " << tujuan << "\n";
}

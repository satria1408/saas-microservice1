#pragma once

#include <memory>
#include <optional>
#include <string>

#include "sqlite3.h"

struct TutupDb {
    void operator()(sqlite3* db) const { sqlite3_close(db); }
};
struct BereskanStmt {
    void operator()(sqlite3_stmt* st) const { sqlite3_finalize(st); }
};
using DbPtr = std::unique_ptr<sqlite3, TutupDb>;
using StmtPtr = std::unique_ptr<sqlite3_stmt, BereskanStmt>;

[[noreturn]] void gagal(sqlite3* db, const std::string& konteks);

DbPtr buka_db(const std::string& path);
StmtPtr siapkan(sqlite3* db, const char* sql);

void bind_teks(sqlite3_stmt* st, int idx, const std::string& s);
void bind_opsional(sqlite3_stmt* st, int idx, const std::optional<std::string>& s);
std::string kolom_teks(sqlite3_stmt* st, int i);

// Jalankan SQL tanpa parameter (BEGIN, COMMIT, dst).
void jalankan(sqlite3* db, const char* sql);

void pastikan_skema(sqlite3* db);
void buat_backup(const std::string& path, int simpan = 5);

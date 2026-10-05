#include "katalog/katalog_skema.h"

#include <string>

#include "db.h"

namespace {

bool kolom_ada(sqlite3* db, const std::string& tabel, const std::string& kolom) {
    auto st = siapkan(db, ("PRAGMA table_info(" + tabel + ")").c_str());
    while (sqlite3_step(st.get()) == SQLITE_ROW)
        if (kolom_teks(st.get(), 1) == kolom) return true;
    return false;
}

void tambah_kolom_jika_belum(sqlite3* db, const std::string& tabel,
                             const std::string& kolom, const std::string& tipe) {
    if (kolom_ada(db, tabel, kolom)) return;
    const std::string sql = "ALTER TABLE " + tabel + " ADD COLUMN " + kolom + " " + tipe;
    jalankan(db, sql.c_str());
}

}  // namespace

void pastikan_skema_katalog(sqlite3* db) {
    jalankan(db,
        "CREATE TABLE IF NOT EXISTS katalog ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " judul TEXT, penulis TEXT, penerbit TEXT, penerbit_sumber TEXT,"
        " stok INTEGER DEFAULT 1,"
        " status_konfirmasi TEXT DEFAULT 'otomatis',"
        " waktu_masuk TEXT DEFAULT CURRENT_TIMESTAMP)");
    // isbn dan kategori ditambahkan belakangan di notebook (DAFTAR_MIGRASI), jadi
    // ditangani seperti kolom tambahan: cek dulu, ALTER hanya kalau belum ada.
    tambah_kolom_jika_belum(db, "katalog", "isbn", "TEXT");
    tambah_kolom_jika_belum(db, "katalog", "kategori", "TEXT");
}
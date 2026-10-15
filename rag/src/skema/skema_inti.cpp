#include "skema_inti.h"

#include <string>

#include "db.h"

namespace {

void tambah_kolom(sqlite3* db, const char* tabel, const char* kolom, const char* tipe) {
    if (punya_kolom(db, tabel, kolom)) return;
    jalankan(db, (std::string("ALTER TABLE ") + tabel + " ADD COLUMN " + kolom + " " + tipe).c_str());
}

}  // namespace

void pastikan_skema_inti(sqlite3* db) {
    // rag_manual: satu-satunya penjaga ISBN. NULL boleh (UNIQUE mengizinkan banyak NULL);
    // kalau terisi harus 13 digit berawalan 978/979 dan unik. Checksum dicek di kode (util.cpp).
    jalankan(db,
        "CREATE TABLE IF NOT EXISTS rag_manual ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " judul TEXT NOT NULL CHECK (length(trim(judul)) > 0),"
        " penulis TEXT,"
        " penerbit TEXT,"
        " isbn TEXT UNIQUE CHECK (isbn IS NULL OR isbn GLOB"
        "   '97[89][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]'),"
        " sumber TEXT DEFAULT 'input_manual',"
        " waktu_masuk TEXT DEFAULT CURRENT_TIMESTAMP)");
    tambah_kolom(db, "rag_manual", "isbn", "TEXT");

    // katalog: staging, sengaja tanpa CHECK karena notebook ikut menulisnya.
    jalankan(db,
        "CREATE TABLE IF NOT EXISTS katalog ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " judul TEXT, penulis TEXT, penerbit TEXT, penerbit_sumber TEXT,"
        " stok INTEGER DEFAULT 1,"
        " status_konfirmasi TEXT DEFAULT 'otomatis',"
        " waktu_masuk TEXT DEFAULT CURRENT_TIMESTAMP)");
    tambah_kolom(db, "katalog", "isbn", "TEXT");
    tambah_kolom(db, "katalog", "kategori", "TEXT");
    jalankan(db,
        "CREATE INDEX IF NOT EXISTS idx_katalog_isbn ON katalog(isbn) WHERE isbn IS NOT NULL");
}
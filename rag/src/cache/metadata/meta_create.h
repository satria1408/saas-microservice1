#pragma once
#include <string>

#include "db.h"
#include "skema/skema_cache.h"
#include "skema/skema_inti.h"
#include "util.h"

// Kunci pencarian, SAMA PERSIS dengan _normalisasi_key() di notebook Python:
std::string buat_key(const std::string& judul, const std::string& penulis);

// Status siklus hidup sebuah entri: baru | ditinjau | dipromosikan | ditolak
bool status_meta_valid(const std::string& s);

// isbn, judul_asli, penulis_asli, status, waktu_masuk.
void pastikan_skema_metadata(sqlite3* db);

// Tabrakan: key sudah dipakai entri LAIN (huruf non-ASCII judul/penulisnya beda). Tidak ditimpa.
enum class HasilMeta { Baru, Update, Lewati, Tabrakan };

// Huruf non-ASCII dalam teks (tanpa tanda baca/simbol, sudah huruf kecil). buat_key() mengubah
// semua non-ASCII jadi '_', jadi dua judul asing yang berbeda bisa menghasilkan key yang sama;
// perbandingan tanda ini yang membedakannya.
std::u32string tanda_huruf_asing(const std::string& utf8);

// true kalau dua pasangan judul/penulis punya huruf non-ASCII yang berbeda.
bool beda_huruf_asing(const std::string& judul_a, const std::string& penulis_a,
                      const std::string& judul_b, const std::string& penulis_b);

// Simpan satu entri (upsert by key). Dipakai CLI dan nanti oleh jalur lain.
// Kalau hasilnya Tabrakan dan penghuni != nullptr, penghuni diisi "judul | penulis" entri yang
// sudah memakai key itu.
HasilMeta meta_simpan(sqlite3* db, const std::string& judul, const std::string& penulis,
                      const std::string& penerbit, const std::string& isbn_mentah,
                      const std::string& sumber, std::string* penghuni = nullptr);

// Pembungkus untuk perintah CLI: rag cache add <judul> <penulis> <penerbit> [isbn]
void meta_tambah(sqlite3* db, const std::string& judul, const std::string& penulis,
                 const std::string& penerbit, const std::string& isbn);
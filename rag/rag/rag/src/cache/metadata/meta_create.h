#pragma once
#include <string>
#include "db.h"

// Kunci pencarian, SAMA PERSIS dengan _normalisasi_key() di notebook Python:
std::string buat_key(const std::string& judul, const std::string& penulis);

// Status siklus hidup sebuah entri: baru | ditinjau | dipromosikan | ditolak
bool status_meta_valid(const std::string& s);

// isbn, judul_asli, penulis_asli, status, waktu_masuk.
void pastikan_skema_metadata(sqlite3* db);

enum class HasilMeta { Baru, Update, Lewati };

// Simpan satu entri (upsert by key). Dipakai CLI dan nanti oleh jalur lain.
HasilMeta meta_simpan(sqlite3* db, const std::string& judul, const std::string& penulis,
                      const std::string& penerbit, const std::string& isbn_mentah,
                      const std::string& sumber);

// Pembungkus untuk perintah CLI: rag cache add <judul> <penulis> <penerbit> [isbn]
void meta_tambah(sqlite3* db, const std::string& judul, const std::string& penulis,
                 const std::string& penerbit, const std::string& isbn);
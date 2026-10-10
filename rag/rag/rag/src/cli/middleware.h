#pragma once
#include "opsi.h"
#include "sqlite3.h"

// Klasifikasi perintah: apa yang disentuhnya.
struct Akses {
    bool baca_saja = false;        // murni membaca, tidak boleh mengubah apa pun
    bool menulis_rag = false;      // add, import, edit, hapus
    bool menulis_cache = false;    // cache add/status/hapus/bersihkan/promosi/ttl, cache scan add/hapus/bersihkan
    bool menulis_katalog = false;  // katalog konfirmasi

    bool menulis() const { return menulis_rag || menulis_cache || menulis_katalog; }
};

Akses tentukan_akses(const Opsi& o);

// Dijalankan setelah DB dibuka, sebelum perintah diarahkan:
//   - baca saja  : PRAGMA query_only = ON (skema juga tidak disentuh)
//   - menulis    : backup DULU, baru skema dipastikan. Dengan begitu migrasi kolom
//                  pertama kali (misalnya isbn) tidak ikut masuk ke salinan
//                  sebelum-perubahan.
//   - migrasi    : dilewati, karena perintah_migrasi mengurus backup dan skema sendiri.
void jalankan_middleware(sqlite3* db, const Opsi& o);
#pragma once
#include "koneksi.h"
#include "opsi.h"

// Apa yang dilakukan sebuah perintah terhadap satu file database.
//   Baca          : harus ada, dibuka SQLITE_OPEN_READONLY (penulisan ditolak di level file)
//   BacaOpsional  : sama, tapi kalau filenya belum ada dibiarkan null
//   Tulis         : dibuka baca-tulis; inti.db di-backup dulu (kecuali --no-backup) baru skema
//                   dipastikan; cache.db dibuat kalau belum ada (tanpa backup, cache boleh hilang)
//   TulisOpsional : dibuka baca-tulis kalau filenya sudah ada, kalau tidak dibiarkan null
//                   (pembersihan cache setelah buku masuk inti)
enum class Peran { Tidak, Baca, BacaOpsional, Tulis, TulisOpsional };

struct Akses {
    Peran inti = Peran::Tidak;
    Peran cache = Peran::Tidak;
};

Akses tentukan_akses(const Opsi& o);

// Membuka file yang dibutuhkan perintah sesuai aksesnya. Perintah yang belum
// diklasifikasikan ditolak. migrasi dan wal mengurus backup/skema sendiri, jadi hanya inti.db
// yang dibuka (migrasi tanpa aksi boleh membuat inti.db baru).
Koneksi siapkan_koneksi(const Opsi& o);
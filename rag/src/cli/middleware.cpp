#include "middleware.h"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

#include "db.h"
#include "skema/skema_cache.h"

Akses tentukan_akses(const Opsi& o) {
    const std::string cmd = o.arg(0);
    const std::string sub = o.arg(1);
    const std::string aksi = o.arg(2);

    Akses a;
    if (cmd == "list") {
        a.inti = Peran::Baca;
    }
    else if (cmd == "add" || cmd == "import") {
        a.inti = Peran::Tulis;
        a.cache = Peran::TulisOpsional;  // buku yang masuk inti dibuang dari cache
    }
    else if (cmd == "edit") {
        a.inti = Peran::Tulis;
        if (o.arg(2) == "isbn") a.cache = Peran::TulisOpsional;  // args: edit <id> <field> <nilai>
    }
    else if (cmd == "hapus") {
        a.inti = Peran::Tulis;
    }
    else if (cmd == "cari" || cmd == "cari-isbn" || cmd == "cari-hash") {
        a.inti = Peran::Baca;
        a.cache = Peran::BacaOpsional;
    }
    else if (cmd == "katalog") {
        if (sub == "list" || sub == "get") a.inti = Peran::Baca;
        else if (sub == "tambah" || sub == "impor") a.inti = Peran::Tulis;
        else if (sub == "konfirmasi") {
            a.inti = Peran::Tulis;
            a.cache = Peran::TulisOpsional;
        }
    }
    else if (cmd == "cache") {
        const bool baca = sub == "list" || sub == "get" ||
            (sub == "kosong" && aksi == "list") ||
            (sub == "scan" && (aksi == "list" || aksi == "get" || aksi == "isbn"));
        const bool tulis = sub == "add" || sub == "hapus" || sub == "bersihkan" ||
            (sub == "kosong" && (aksi == "add" || aksi == "hapus")) ||
            (sub == "scan" && (aksi == "add" || aksi == "hapus" || aksi == "bersihkan"));
        if (baca) a.cache = Peran::Baca;
        else if (tulis) a.cache = Peran::Tulis;
    }
    return a;
}

Koneksi siapkan_koneksi(const Opsi& o) {
    const std::string cmd = o.arg(0);
    Koneksi k;

    if (cmd == "migrasi" || cmd == "wal") {
        const bool buat = cmd == "migrasi" && o.arg(1).empty();
        k.inti = buka_db(o.db, buat ? ModeBuka::Buat : ModeBuka::BacaTulis);
        return k;
    }

    const Akses a = tentukan_akses(o);
    if (a.inti == Peran::Tidak && a.cache == Peran::Tidak)
        throw std::runtime_error("perintah '" + cmd + "' belum diklasifikasikan sebagai baca/tulis "
                                 "(periksa tentukan_akses di middleware.cpp)");

    if (a.inti == Peran::Baca) {
        k.inti = buka_db(o.db, ModeBuka::HanyaBaca);
    } else if (a.inti == Peran::Tulis) {
        k.inti = buka_db(o.db);  // harus sudah ada: salah ketik path tidak membuat DB kosong baru
        // Backup DULU, baru skema dipastikan, supaya migrasi kolom pertama kali tidak ikut
        // masuk ke salinan sebelum-perubahan.
        if (!o.no_backup) buat_backup(k.inti.get(), o.db, o.simpan_backup);
        pastikan_skema(k.inti.get());
    }

    std::error_code ec;
    const bool cache_ada = std::filesystem::exists(o.cache, ec);
    switch (a.cache) {
        case Peran::Baca:
            if (!cache_ada)
                throw std::runtime_error("cache.db tidak ditemukan: " + o.cache +
                                         " (buat dengan: rag migrasi, atau tentukan --cache)");
            k.cache = buka_db(o.cache, ModeBuka::HanyaBaca);
            break;
        case Peran::BacaOpsional:
            if (cache_ada) k.cache = buka_db(o.cache, ModeBuka::HanyaBaca);
            break;
        case Peran::Tulis:
            k.cache = buka_db(o.cache, ModeBuka::Buat);
            pastikan_skema_cache(k.cache.get());
            break;
        case Peran::TulisOpsional:
            if (cache_ada) k.cache = buka_db(o.cache);
            break;
        case Peran::Tidak:
            break;
    }
    return k;
}
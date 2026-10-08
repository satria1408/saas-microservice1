#include <cctype>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "alur/alur_cari.h"
#include "cache/metadata/meta_create.h"
#include "cache/metadata/meta_kelola.h"
#include "cache/metadata/meta_promosi.h"
#include "cache/metadata/meta_read.h"
#include "cache/metadata/meta_ttl.h"
#include "cache/scan/scan_bersihkan.h"
#include "cache/scan/scan_create.h"
#include "cache/scan/scan_hapus.h"
#include "cache/scan/scan_read.h"
#include "cache/scan/scan_skema.h"
#include "crud/create.h"
#include "crud/delete.h"
#include "crud/read.h"
#include "crud/update.h"
#include "db.h"
#include "katalog/katalog_read.h"
#include "katalog/katalog_skema.h"
#include "katalog/katalog_ubah.h"

struct Opsi {
    std::string db;
    bool no_backup = false, tanpa_transaksi = false, rinci = false, yes = false;
    int hari = 0; 
    int simpan_backup = 5;
    std::vector<std::string> args;
};

static int baca_angka(const std::string& s, const char* nama) {
    if (s.empty() || s.size() > 6 || s.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error(std::string(nama) + " butuh angka (maksimal 6 digit)");
    return std::stoi(s);
}

static Opsi baca_opsi(int argc, char** argv) {
    Opsi o;
    if (const char* e = std::getenv("RAG_DB")) o.db = e;
    else o.db = "book_cache.db";
    for (int k = 1; k < argc; ++k) {
        std::string a = argv[k];
        if (a == "--db") {
            if (k + 1 >= argc) throw std::runtime_error("--db butuh path");
            o.db = argv[++k];
        } else if (a == "--hari") {
            if (k + 1 >= argc) throw std::runtime_error("--hari butuh angka");
            o.hari = baca_angka(argv[++k], "--hari");
        } else if (a == "--simpan-backup") {
            if (k + 1 >= argc) throw std::runtime_error("--simpan-backup butuh angka");
            o.simpan_backup = baca_angka(argv[++k], "--simpan-backup");
            if (o.simpan_backup < 1) throw std::runtime_error("--simpan-backup minimal 1");
        } else if (a == "--no-backup") o.no_backup = true;
        else if (a == "--tanpa-transaksi") o.tanpa_transaksi = true;
        else if (a == "--rinci") o.rinci = true;
        else if (a == "-y") o.yes = true;
        else if (a.size() > 1 && a[0] == '-' && !std::isdigit(static_cast<unsigned char>(a[1])))
            throw std::runtime_error("opsi tidak dikenal: " + a);
        else o.args.push_back(a);
    }
    return o;
}

static long long baca_id(const std::string& s, const char* perintah) {
    if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error(std::string(perintah) + " butuh <id> berupa angka");
    return std::stoll(s);
}

static const std::string& wajib(const std::string& nilai, const char* pesan) {
    if (nilai.empty()) throw std::runtime_error(pesan);
    return nilai;
}

// Cek BENTUK argumen: apa yang wajib ada, mana yang harus angka. Dijalankan sebelum DB
// dibuka dan sebelum backup dibuat, jadi salah ketik tidak menghasilkan backup baru (dan
// tidak membuat rotasi membuang backup lama). Pesan errornya sama dengan yang dilempar
// fungsi-fungsi di bawah; cek di sana tetap ada sebagai jaring kedua.
//
// Yang BUKAN cek bentuk (masih terjadi setelah backup): id yang tidak ada di tabel, ISBN
// yang checksum-nya salah, stok yang bukan angka di katalog konfirmasi, hash yang tidak
// ketemu. Itu butuh membaca DB atau ada di dalam modulnya masing-masing.
static void validasi_bentuk(const Opsi& o) {
    auto arg = [&](size_t i) { return i < o.args.size() ? o.args[i] : std::string(); };
    const std::string cmd = arg(0);
    const std::string sub = arg(1);

    if (cmd == "edit" || cmd == "hapus") {
        baca_id(arg(1), cmd.c_str());
    }
    else if (cmd == "import") {
        wajib(arg(1), "import butuh <file.csv>");
    }
    else if (cmd == "katalog" && sub == "konfirmasi") {
        baca_id(wajib(arg(2), "katalog konfirmasi butuh <id>"), "katalog konfirmasi");
    }
    else if (cmd == "cache") {
        if (sub == "status") {
            wajib(arg(2), "cache status butuh <key> <status>");
            wajib(arg(3), "cache status butuh <key> <status>");
        }
        else if (sub == "hapus") {
            wajib(arg(2), "cache hapus butuh <key>");
        }
        else if (sub == "promosi") {
            wajib(arg(2), "cache promosi butuh <key>");
        }
        else if (sub == "ttl" && arg(2) == "pasang") {
            if (!arg(3).empty()) baca_angka(arg(3), "hari ditolak");
            if (!arg(4).empty()) baca_angka(arg(4), "hari dipromosikan");
            if (!arg(5).empty()) baca_angka(arg(5), "hari baru");
        }
        else if (sub == "scan") {
            const std::string aksi = arg(2);
            if (aksi == "add")
                wajib(arg(3), "cache scan add butuh <hash> <judul> <penulis> [kategori]");
            else if (aksi == "hapus")
                wajib(arg(3), "cache scan hapus butuh <awalan-hash>");
            else if (aksi == "bersihkan" && o.hari < 1)
                throw std::runtime_error("cache scan bersihkan butuh --hari N (N minimal 1)");
        }
    }
}

// Semua perintah 'rag cache scan <aksi> ...'. args[0]="cache", args[1]="scan", args[2]=aksi.
static void perintah_cache_scan(sqlite3* db, const Opsi& o) {
    auto arg = [&](size_t i) { return i < o.args.size() ? o.args[i] : std::string(); };
    const std::string aksi = arg(2);

    if (aksi == "list") {
        scan_daftar(db, arg(3));
    }
    else if (aksi == "get") {
        scan_tampil(db, wajib(arg(3), "cache scan get butuh <hash>"));
    }
    else if (aksi == "add") {
        scan_tambah(db, wajib(arg(3), "cache scan add butuh <hash> <judul> <penulis> [kategori]"),
                    arg(4), arg(5), arg(6));
    }
    else if (aksi == "hapus") {
        scan_hapus(db, wajib(arg(3), "cache scan hapus butuh <awalan-hash>"), o.yes);
    }
    else if (aksi == "bersihkan") {
        scan_bersihkan(db, o.hari, o.yes);
    }
    else if (aksi.empty()) throw std::runtime_error("cache scan butuh aksi: list, get, add, hapus, atau bersihkan");
    else throw std::runtime_error("aksi cache scan tidak dikenal: " + aksi);
}

// Semua perintah 'rag cache <sub> ...'. args[0]="cache", args[1]=sub.
static void perintah_cache(sqlite3* db, const Opsi& o) {
    auto arg = [&](size_t i) { return i < o.args.size() ? o.args[i] : std::string(); };
    const std::string sub = arg(1);

    if (sub == "list") {
        // Argumen pertama dianggap status kalau cocok salah satu status valid,
        // selain itu dianggap kata kunci.
        std::string status, kata;
        if (status_meta_valid(arg(2))) { status = arg(2); kata = arg(3); }
        else kata = arg(2);
        meta_daftar(db, status, kata);
    }
    else if (sub == "add") {  // hapus baris ini kalau meta_tambah sudah kamu buang
        meta_tambah(db, arg(2), arg(3), arg(4), arg(5));
    }
    else if (sub == "status") {
        meta_set_status(db, wajib(arg(2), "cache status butuh <key> <status>"),
                        wajib(arg(3), "cache status butuh <key> <status>"));
    }
    else if (sub == "hapus") {
        meta_hapus(db, wajib(arg(2), "cache hapus butuh <key>"), o.yes);
    }
    else if (sub == "bersihkan") {
        meta_bersihkan(db, o.hari, o.yes);
    }
    else if (sub == "ttl") {
        const std::string aksi = arg(2);
        if (aksi.empty()) meta_status_ttl(db);
        else if (aksi == "pasang") {
            int ditolak = arg(3).empty() ? 7 : baca_angka(arg(3), "hari ditolak");
            int promosi = arg(4).empty() ? 0 : baca_angka(arg(4), "hari dipromosikan");
            int baru    = arg(5).empty() ? 90 : baca_angka(arg(5), "hari baru");
            meta_pasang_ttl(db, ditolak, promosi, baru);
        }
        else if (aksi == "lepas") meta_lepas_ttl(db);
        else throw std::runtime_error("cache ttl: aksi harus pasang atau lepas");
    }
    else if (sub == "promosi") {
        meta_promosi(db, wajib(arg(2), "cache promosi butuh <key>"), arg(3), arg(4));
    }
    else if (sub == "scan") {
        perintah_cache_scan(db, o);
    }
    else if (sub.empty()) throw std::runtime_error("cache butuh sub-perintah (jalankan rag tanpa argumen)");
    else throw std::runtime_error("sub-perintah cache tidak dikenal: " + sub);
}

// Semua perintah 'rag katalog <aksi> ...'. args[0]="katalog", args[1]=aksi.
static void perintah_katalog(sqlite3* db, const Opsi& o) {
    auto arg = [&](size_t i) { return i < o.args.size() ? o.args[i] : std::string(); };
    const std::string aksi = arg(1);

    if (aksi == "list") {
        // Argumen pertama dianggap status kalau cocok otomatis/terkonfirmasi,
        // selain itu dianggap kata kunci.
        std::string status, kata;
        if (status_katalog_valid(arg(2))) { status = arg(2); kata = arg(3); }
        else kata = arg(2);
        katalog_daftar(db, status, kata);
    }
    else if (aksi == "get") {
        katalog_tampil(db, baca_id(wajib(arg(2), "katalog get butuh <id>"), "katalog get"));
    }
    else if (aksi == "konfirmasi") {
        katalog_konfirmasi(db,
            baca_id(wajib(arg(2), "katalog konfirmasi butuh <id>"), "katalog konfirmasi"),
            arg(3), arg(4), arg(5), o.yes);
    }
    else if (aksi.empty()) throw std::runtime_error("katalog butuh aksi: list, get, atau konfirmasi");
    else throw std::runtime_error("aksi katalog tidak dikenal: " + aksi);
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    try {
        Opsi o = baca_opsi(argc, argv);
        if (o.args.empty()) {
            std::cerr <<
                "Pakai: rag [--db path] add <judul> <penulis> <penerbit> [isbn]\n"
                "       rag [--db path] import file.csv [--tanpa-transaksi] [--rinci]\n"
                "       rag [--db path] list [kata]\n"
                "       rag [--db path] edit <id> <judul|penulis|penerbit|isbn> <nilai>\n"
                "       rag [--db path] hapus <id> [-y]\n"
                "\n"
                "       rag [--db path] cache list [baru|ditinjau|dipromosikan|ditolak] [kata]\n"
                "       rag [--db path] cache status <key> <baru|ditinjau|ditolak>\n"
                "       rag [--db path] cache hapus <key> [-y]\n"
                "       rag [--db path] cache bersihkan [--hari N] [-y]\n"
                "       rag [--db path] cache promosi <key> [penerbit] [isbn]\n"
                "       rag [--db path] cache add <judul> <penulis> <penerbit> [isbn]\n"
                "\n"
                "       rag [--db path] cache scan list [kata]\n"
                "       rag [--db path] cache scan get <awalan-hash>\n"
                "       rag [--db path] cache scan add <hash> <judul> <penulis> [kategori]\n"
                "       rag [--db path] cache scan hapus <awalan-hash> [-y]\n"
                "       rag [--db path] cache scan bersihkan --hari N [-y]\n"
                "\n"
                "       rag [--db path] katalog list [otomatis|terkonfirmasi] [kata]\n"
                "       rag [--db path] katalog get <id>\n"
                "       rag [--db path] katalog konfirmasi <id> [penerbit] [isbn] [stok] [-y]\n"
                "                       (\"-\" atau kosong = tidak diubah)\n"
                "\n"
                "       rag [--db path] cari <judul> [penulis]   (telusuri cache_metadata, lalu rag_manual)\n"
                "       rag [--db path] cari-isbn <isbn>         (telusuri lewat ISBN)\n"
                "\n"
                "       rag [--db path] migrasi    (backup dulu, lalu mutakhirkan skema semua tabel)\n"
                "\n"
                "Opsi: --no-backup\n";
            return 1;
        }

        // Cek bentuk argumen DULU, sebelum DB dibuka dan sebelum backup dibuat.
        validasi_bentuk(o);

        const std::string cmd = o.args[0];
        auto arg = [&](size_t i) { return i < o.args.size() ? o.args[i] : std::string(); };
        const std::string sub = arg(1);

        const bool menulis_katalog = (cmd == "katalog") && (sub == "konfirmasi");

        // Perintah yang murni membaca. Tidak boleh mengubah apa pun, termasuk skema.
        const bool baca_saja = (cmd == "list") || (cmd == "cari") || (cmd == "cari-isbn") ||
            (cmd == "katalog" && !menulis_katalog) ||
            (cmd == "cache" && (sub == "list" ||
                                (sub == "scan" && (arg(2) == "list" || arg(2) == "get"))));

        const bool menulis_cache = (cmd == "cache") &&
            (sub == "add" || sub == "status" || sub == "hapus" ||
             sub == "bersihkan" || sub == "promosi" ||
             (sub == "ttl" && (arg(2) == "pasang" || arg(2) == "lepas")) ||
             (sub == "scan" && (arg(2) == "add" || arg(2) == "hapus" ||
                                arg(2) == "bersihkan")));
        const bool menulis_rag = (cmd == "add" || cmd == "import" || cmd == "edit" || cmd == "hapus");

        DbPtr db = buka_db(o.db);

        if (cmd == "migrasi") {
            if (!o.no_backup) buat_backup(o.db, o.simpan_backup);
            pastikan_skema(db.get());
            pastikan_skema_metadata(db.get());
            pastikan_skema_scan(db.get());
            pastikan_skema_katalog(db.get());
            std::cout << "[OK] skema rag_manual, cache_metadata, cache_scan, dan katalog sudah mutakhir\n";
            return 0;
        }

        if (baca_saja) {
            jalankan(db.get(), "PRAGMA query_only = ON");
        } else {
            // Backup DULU, baru skema dipastikan. Dengan begitu migrasi kolom pertama
            // kali (misalnya isbn) tidak ikut masuk ke salinan sebelum-perubahan.
            if ((menulis_cache || menulis_rag || menulis_katalog) && !o.no_backup)
                buat_backup(o.db, o.simpan_backup);
            pastikan_skema(db.get());
            if (cmd == "cache") {
                pastikan_skema_metadata(db.get());
                pastikan_skema_scan(db.get());
            }
            if (cmd == "katalog") pastikan_skema_katalog(db.get());
        }

        if (cmd == "cari") {
            cetak_hasil(cari_metadata(db.get(), wajib(arg(1), "cari butuh <judul> [penulis]"), arg(2)));
            return 0;
        }

        if (cmd == "cari-isbn") {
            cetak_hasil(cari_dari_isbn_lokal(db.get(), wajib(arg(1), "cari-isbn butuh <isbn>")));
            return 0;
        }

        if (cmd == "katalog") {
            perintah_katalog(db.get(), o);
            return 0;
        }

        if (cmd == "cache") {
            perintah_cache(db.get(), o);
            return 0;
        }

        if (cmd == "add")         tambah(db.get(), arg(1), arg(2), arg(3), arg(4));
        else if (cmd == "import") {
            if (arg(1).empty()) throw std::runtime_error("import butuh <file.csv>");
            impor(db.get(), arg(1), !o.tanpa_transaksi, o.rinci);
        }
        else if (cmd == "list")   daftar(db.get(), arg(1));
        else if (cmd == "edit")   edit(db.get(), baca_id(arg(1), "edit"), arg(2), arg(3));
        else if (cmd == "hapus")  hapus(db.get(), baca_id(arg(1), "hapus"), o.yes);
        else throw std::runtime_error("perintah tidak dikenal: " + cmd);
    } catch (const std::exception& e) {
        const std::string pesan = e.what();
        std::cerr << "[ERROR] " << pesan << "\n";
        if (pesan.find("no such table") != std::string::npos ||
            pesan.find("no such column") != std::string::npos)
            std::cerr << "        Skema DB belum mutakhir. Jalankan: rag --db <path> migrasi\n";
        return 1;
    }
    return 0;
}
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "cache/metadata/meta_create.h"
#include "cache/metadata/meta_kelola.h"
#include "cache/metadata/meta_promosi.h"
#include "cache/metadata/meta_read.h"
#include "crud/create.h"
#include "crud/delete.h"
#include "crud/read.h"
#include "crud/update.h"
#include "db.h"

struct Opsi {
    std::string db;
    bool no_backup = false, tanpa_transaksi = false, rinci = false, yes = false;
    int hari = 0; 
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
    else if (sub == "promosi") {
        meta_promosi(db, wajib(arg(2), "cache promosi butuh <key>"), arg(3), arg(4));
    }
    else if (sub.empty()) throw std::runtime_error("cache butuh sub-perintah (jalankan rag tanpa argumen)");
    else throw std::runtime_error("sub-perintah cache tidak dikenal: " + sub);
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
                "Opsi: --no-backup\n";
            return 1;
        }
        const std::string cmd = o.args[0];
        auto arg = [&](size_t i) { return i < o.args.size() ? o.args[i] : std::string(); };

        DbPtr db = buka_db(o.db);
        pastikan_skema(db.get());

        if (cmd == "cache") {
            const std::string sub = arg(1);
            const bool menulis_cache = (sub == "add" || sub == "status" || sub == "hapus" ||
                                        sub == "bersihkan" || sub == "promosi");
            // Backup DULU, baru skema cache dipastikan, supaya migrasi kolom
            // pertama kali ikut tercatat di salinan sebelum-perubahan.
            if (menulis_cache && !o.no_backup) buat_backup(o.db);
            pastikan_skema_metadata(db.get());
            perintah_cache(db.get(), o);
            return 0;
        }

        bool menulis = (cmd == "add" || cmd == "import" || cmd == "edit" || cmd == "hapus");
        if (menulis && !o.no_backup) buat_backup(o.db);

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
        std::cerr << "[ERROR] " << e.what() << "\n";
        return 1;
    }
    return 0;
}
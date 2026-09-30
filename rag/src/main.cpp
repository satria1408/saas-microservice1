// Titik masuk: baca opsi command line lalu serahkan ke file CRUD yang sesuai.
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "create.h"
#include "db.h"
#include "delete.h"
#include "read.h"
#include "update.h"

struct Opsi {
    std::string db;
    bool no_backup = false, tanpa_transaksi = false, rinci = false, yes = false;
    std::vector<std::string> args;
};

static Opsi baca_opsi(int argc, char** argv) {
    Opsi o;
    if (const char* e = std::getenv("RAG_DB")) o.db = e;
    else o.db = "book_cache.db";
    for (int k = 1; k < argc; ++k) {
        std::string a = argv[k];
        if (a == "--db") {
            if (k + 1 >= argc) throw std::runtime_error("--db butuh path");
            o.db = argv[++k];
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

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    try {
        Opsi o = baca_opsi(argc, argv);
        if (o.args.empty()) {
            std::cerr << "Pakai: rag [--db path] add <judul> <penulis> <penerbit> [isbn]\n"
                         "       rag [--db path] import file.csv [--tanpa-transaksi] [--rinci]\n"
                         "       rag [--db path] list [kata]\n"
                         "       rag [--db path] edit <id> <judul|penulis|penerbit|isbn> <nilai>\n"
                         "       rag [--db path] hapus <id> [-y]\n"
                         "Opsi: --no-backup\n";
            return 1;
        }
        const std::string cmd = o.args[0];
        auto arg = [&](size_t i) { return i < o.args.size() ? o.args[i] : std::string(); };

        DbPtr db = buka_db(o.db);
        pastikan_skema(db.get());

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

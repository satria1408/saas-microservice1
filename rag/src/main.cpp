#include <iostream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

#include "bantuan.h"
#include "db.h"
#include "middleware.h"
#include "opsi.h"
#include "router.h"
#include "validasi.h"

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    try {
        Opsi o = baca_opsi(argc, argv);
        if (o.args.empty()) {
            cetak_bantuan();
            return 1;
        }

        // Cek bentuk argumen DULU, sebelum DB dibuka dan sebelum backup dibuat.
        validasi_bentuk(o);

        DbPtr db = buka_db(o.db);
        jalankan_middleware(db.get(), o);  // query_only / backup + skema
        arahkan(db.get(), o);              // router -> kontrol/*
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
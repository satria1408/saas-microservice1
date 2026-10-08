#include "validasi.h"

#include <stdexcept>
#include <string>

void validasi_bentuk(const Opsi& o) {
    const std::string cmd = o.arg(0);
    const std::string sub = o.arg(1);

    if (cmd == "edit" || cmd == "hapus") {
        baca_id(o.arg(1), cmd.c_str());
    }
    else if (cmd == "import") {
        wajib(o.arg(1), "import butuh <file.csv>");
    }
    else if (cmd == "katalog" && sub == "konfirmasi") {
        baca_id(wajib(o.arg(2), "katalog konfirmasi butuh <id>"), "katalog konfirmasi");
    }
    else if (cmd == "cache") {
        if (sub == "status") {
            wajib(o.arg(2), "cache status butuh <key> <status>");
            wajib(o.arg(3), "cache status butuh <key> <status>");
        }
        else if (sub == "hapus") {
            wajib(o.arg(2), "cache hapus butuh <key>");
        }
        else if (sub == "promosi") {
            wajib(o.arg(2), "cache promosi butuh <key>");
        }
        else if (sub == "ttl" && o.arg(2) == "pasang") {
            if (!o.arg(3).empty()) baca_angka(o.arg(3), "hari ditolak");
            if (!o.arg(4).empty()) baca_angka(o.arg(4), "hari dipromosikan");
            if (!o.arg(5).empty()) baca_angka(o.arg(5), "hari baru");
        }
        else if (sub == "scan") {
            const std::string aksi = o.arg(2);
            if (aksi == "add")
                wajib(o.arg(3), "cache scan add butuh <hash> <judul> <penulis> [kategori]");
            else if (aksi == "hapus")
                wajib(o.arg(3), "cache scan hapus butuh <awalan-hash>");
            else if (aksi == "bersihkan" && o.hari < 1)
                throw std::runtime_error("cache scan bersihkan butuh --hari N (N minimal 1)");
        }
    }
}
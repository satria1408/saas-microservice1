#include "validasi.h"

#include <initializer_list>
#include <stdexcept>
#include <string>

namespace {

bool salah_satu(const std::string& s, std::initializer_list<const char*> daftar) {
    for (const char* d : daftar)
        if (s == d) return true;
    return false;
}

// Tolak nama perintah yang tidak dikenal SEBELUM DB dibuka. Tanpa ini, middleware sempat
// menjalankan pastikan_skema* (CREATE TABLE / ALTER) untuk perintah yang ternyata salah ketik.
// Pesannya sama dengan yang dilempar router dan kontrol (yang tetap ada sebagai jaring kedua).
void validasi_nama(const Opsi& o) {
    const std::string cmd = o.arg(0);
    const std::string sub = o.arg(1);
    const std::string aksi = o.arg(2);

    if (!salah_satu(cmd, {"add", "import", "list", "edit", "hapus", "cache",
                          "katalog", "cari", "cari-isbn", "migrasi"}))
        throw std::runtime_error("perintah tidak dikenal: " + cmd);

    if (cmd == "katalog") {
        if (sub.empty()) throw std::runtime_error("katalog butuh aksi: list, get, atau konfirmasi");
        if (!salah_satu(sub, {"list", "get", "konfirmasi"}))
            throw std::runtime_error("aksi katalog tidak dikenal: " + sub);
    }
    else if (cmd == "cache") {
        if (sub.empty())
            throw std::runtime_error("cache butuh sub-perintah (jalankan rag tanpa argumen)");
        if (!salah_satu(sub, {"list", "add", "status", "hapus", "bersihkan", "ttl", "promosi", "scan"}))
            throw std::runtime_error("sub-perintah cache tidak dikenal: " + sub);
        if (sub == "ttl" && !aksi.empty() && !salah_satu(aksi, {"pasang", "lepas"}))
            throw std::runtime_error("cache ttl: aksi harus pasang atau lepas");
        if (sub == "scan") {
            if (aksi.empty())
                throw std::runtime_error("cache scan butuh aksi: list, get, add, hapus, atau bersihkan");
            if (!salah_satu(aksi, {"list", "get", "add", "hapus", "bersihkan"}))
                throw std::runtime_error("aksi cache scan tidak dikenal: " + aksi);
        }
    }
}

}  // namespace

void validasi_bentuk(const Opsi& o) {
    validasi_nama(o);

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
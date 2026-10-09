#include "validasi.h"
#include "util.h"

#include <initializer_list>
#include <stdexcept>
#include <string>

namespace {

bool salah_satu(const std::string& s, std::initializer_list<const char*> daftar) {
    for (const char* d : daftar)
        if (s == d) return true;
    return false;
}

// ISBN-13 yang valid: awalan 978/979 dan checksum benar. Dipakai untuk masukan yang
// WAJIB valid (scan add).
void cek_isbn(const std::string& mentah) {
    const auto b = bersihkan_isbn(mentah);
    if (!b || !isbn13_valid(*b))
        throw std::runtime_error("ISBN '" + trim(mentah) +
                                 "' bukan ISBN-13 yang valid (awalan 978/979 dan checksum benar)");
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
                throw std::runtime_error("cache scan butuh aksi: list, get, isbn, add, hapus, atau bersihkan");
            if (!salah_satu(aksi, {"list", "get", "isbn", "add", "hapus", "bersihkan"}))
                throw std::runtime_error("aksi cache scan tidak dikenal: " + aksi);
        }
    }
}

}  // namespace

void validasi_bentuk(const Opsi& o) {
    validasi_nama(o);

    const std::string cmd = o.arg(0);
    const std::string sub = o.arg(1);

    if (cmd == "edit") {
        baca_id(o.arg(1), "edit");
        const std::string field = o.arg(2);
        const std::string nilai = o.arg(3);
        // Sama dengan edit() di crud/update.cpp.
        if (!salah_satu(field, {"judul", "penulis", "penerbit", "isbn"}))
            throw std::runtime_error("kolom harus: judul, penulis, penerbit, atau isbn");
        if (field == "isbn") {
            const auto b = bersihkan_isbn(nilai);  // kosong = boleh (mengosongkan ISBN)
            if (b && !ean13_valid(*b)) throw std::runtime_error("ISBN tidak lolos checksum");
        }
        else if (field == "judul" && trim(nilai).empty()) {
            throw std::runtime_error("judul tidak boleh kosong");
        }
    }
    else if (cmd == "hapus") {
        baca_id(o.arg(1), "hapus");
    }
    else if (cmd == "import") {
        wajib(o.arg(1), "import butuh <file.csv>");
    }
    else if (cmd == "katalog" && sub == "konfirmasi") {
        baca_id(wajib(o.arg(2), "katalog konfirmasi butuh <id>"), "katalog konfirmasi");
    }
    else if (cmd == "cache") {
        if (sub == "add") {
            // Sama dengan meta_tambah().
            if (trim(o.arg(2)).empty()) throw std::runtime_error("cache add butuh minimal <judul>");
        }
        else if (sub == "status") {
            wajib(o.arg(2), "cache status butuh <key> <status>");
            const std::string status = wajib(o.arg(3), "cache status butuh <key> <status>");
            // Sama dengan meta_set_status().
            if (status == "dipromosikan")
                throw std::runtime_error("status 'dipromosikan' hanya diatur lewat perintah promosi");
            if (!salah_satu(status, {"baru", "ditinjau", "ditolak"}))
                throw std::runtime_error("status harus: baru, ditinjau, atau ditolak");
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
            if (aksi == "add") {
                wajib(o.arg(3), "cache scan add butuh <hash> <judul> <penulis> [kategori] [isbn]");
                if (!trim(o.arg(7)).empty()) cek_isbn(o.arg(7));
            }
            else if (aksi == "isbn")
                wajib(o.arg(3), "cache scan isbn butuh <isbn>");
            else if (aksi == "hapus")
                wajib(o.arg(3), "cache scan hapus butuh <awalan-hash>");
            else if (aksi == "bersihkan" && o.hari < 1)
                throw std::runtime_error("cache scan bersihkan butuh --hari N (N minimal 1)");
        }
    }
}
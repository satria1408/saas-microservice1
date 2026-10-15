#include "validasi.h"
#include "util.h"

#include <fstream>
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
                          "katalog", "cari", "cari-isbn", "cari-hash", "migrasi", "wal"}))
        throw std::runtime_error("perintah tidak dikenal: " + cmd);

    if (cmd == "katalog") {
        if (sub.empty()) throw std::runtime_error("katalog butuh aksi: list, get, tambah, impor, atau konfirmasi");
        if (!salah_satu(sub, {"list", "get", "tambah", "impor", "konfirmasi"}))
            throw std::runtime_error("aksi katalog tidak dikenal: " + sub);
    }
    else if (cmd == "wal") {
        if (!sub.empty() && !salah_satu(sub, {"pasang", "lepas"}))
            throw std::runtime_error("aksi wal tidak dikenal: " + sub);
        if (!aksi.empty()) throw std::runtime_error("wal tidak memakai argumen tambahan: " + aksi);
    }
    else if (cmd == "cache") {
        if (sub.empty())
            throw std::runtime_error("cache butuh sub-perintah (jalankan rag tanpa argumen)");
        if (!salah_satu(sub, {"list", "get", "add", "hapus", "kosong", "bersihkan", "scan"}))
            throw std::runtime_error("sub-perintah cache tidak dikenal: " + sub);
        if (sub == "kosong") {
            if (aksi.empty()) throw std::runtime_error("cache kosong butuh aksi: add, list, atau hapus");
            if (!salah_satu(aksi, {"add", "list", "hapus"}))
                throw std::runtime_error("aksi cache kosong tidak dikenal: " + aksi);
        }
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

    if (o.paksa && !(cmd == "wal" && sub == "pasang"))
        throw std::runtime_error("opsi --paksa hanya untuk: wal pasang");

    if (cmd == "edit") {
        baca_id(o.arg(1), "edit");
        const std::string field = o.arg(2);
        const std::string nilai = o.arg(3);
        // Sama dengan edit() di crud/update.cpp.
        if (!salah_satu(field, {"judul", "penulis", "penerbit", "isbn"}))
            throw std::runtime_error("kolom harus: judul, penulis, penerbit, atau isbn");
        if (field == "isbn") {
            const auto b = bersihkan_isbn(nilai);  // kosong = boleh (mengosongkan ISBN)
            if (b && !isbn13_valid(*b))
                throw std::runtime_error("ISBN bukan ISBN-13 yang valid (awalan 978/979 dan checksum benar)");
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
        // Sebelum DB dibuka: kalau file tidak ada, jangan sampai ada backup yang dibuat (dan
        // backup lama dirotasi) untuk perintah yang pasti gagal. Pesan sama dengan csv.cpp.
        if (!std::ifstream(o.arg(1), std::ios::binary))
            throw std::runtime_error("Tidak bisa membuka CSV: " + o.arg(1));
    }
    else if (cmd == "katalog" && sub == "tambah") {
        wajib(o.arg(2), "katalog tambah butuh <judul> [penulis] [penerbit] [isbn] [stok]");
        if (!o.arg(6).empty()) baca_angka(o.arg(6), "stok");
    }
    else if (cmd == "katalog" && sub == "impor") {
        wajib(o.arg(2), "katalog impor butuh <file.csv>");
        if (!std::ifstream(o.arg(2), std::ios::binary))
            throw std::runtime_error("Tidak bisa membuka CSV: " + o.arg(2));
    }
    else if (cmd == "katalog" && sub == "konfirmasi") {
        baca_id(wajib(o.arg(2), "katalog konfirmasi butuh <id>"), "katalog konfirmasi");
    }
    else if (cmd == "cari-hash") {
        wajib(o.arg(1), "cari-hash butuh <hash atau awalannya>");
    }
    else if (cmd == "cache") {
        const std::string aksi = o.arg(2);
        if (sub == "get") {
            cek_isbn(wajib(o.arg(2), "cache get butuh <isbn>"));
        }
        else if (sub == "add") {
            cek_isbn(wajib(o.arg(2), "cache add butuh <isbn> <judul> [penulis] [penerbit] [sumber]"));
            if (trim(o.arg(3)).empty())
                throw std::runtime_error("cache add butuh <isbn> <judul> [penulis] [penerbit] [sumber]");
        }
        else if (sub == "hapus") {
            cek_isbn(wajib(o.arg(2), "cache hapus butuh <isbn>"));
        }
        else if (sub == "kosong" && (aksi == "add" || aksi == "hapus")) {
            cek_isbn(wajib(o.arg(3), aksi == "add" ? "cache kosong add butuh <isbn>"
                                                   : "cache kosong hapus butuh <isbn>"));
        }
        else if (sub == "scan") {
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
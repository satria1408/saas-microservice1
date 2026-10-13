#include "kontrol/wal_kontrol.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>

#include "db.h"
#include "util.h"

namespace {

std::string teks_pragma(sqlite3* db, const char* sql) {
    auto st = siapkan(db, sql);
    return sqlite3_step(st.get()) == SQLITE_ROW ? kolom_teks(st.get(), 0) : "?";
}

std::string label_sync(const std::string& n) {
    if (n == "0") return "0 (OFF)";
    if (n == "1") return "1 (NORMAL)";
    if (n == "2") return "2 (FULL)";
    if (n == "3") return "3 (EXTRA)";
    return n;
}

std::string ukuran_file(const std::string& p) {
    std::error_code ec;
    if (!std::filesystem::exists(p, ec)) return "tidak ada";
    const auto sz = std::filesystem::file_size(p, ec);
    return ec ? std::string("?") : std::to_string(sz) + " byte";
}

void tampil_status(sqlite3* db, const Opsi& o) {
    const std::string mode = huruf_kecil(teks_pragma(db, "PRAGMA journal_mode"));
    const std::string alasan = lokasi_berisiko(o.db);
    std::cout << "journal_mode   : " << mode << "\n"
              << "synchronous    : " << label_sync(teks_pragma(db, "PRAGMA synchronous")) << "  (koneksi ini)\n";
    if (mode == "wal")
        std::cout << "auto-checkpoint: " << teks_pragma(db, "PRAGMA wal_autocheckpoint") << " halaman\n";
    std::cout << "file -wal      : " << ukuran_file(o.db + "-wal") << "\n"
              << "file -shm      : " << ukuran_file(o.db + "-shm") << "\n"
              << "lokasi         : " << (alasan.empty() ? "aman untuk WAL" : "BERISIKO - " + alasan) << "\n"
              << "versi SQLite   : " << sqlite3_libversion() << "\n";
}

void pasang(sqlite3* db, const Opsi& o) {
    const std::string mode = huruf_kecil(teks_pragma(db, "PRAGMA journal_mode"));
    if (mode == "wal") {
        std::cout << "[OK] sudah WAL, tidak ada yang diubah\n";
        return;
    }
    // Cek lokasi DULU, sebelum backup: penolakan tidak boleh membuat/merotasi backup.
    if (!o.paksa) {
        const std::string alasan = lokasi_berisiko(o.db);
        if (!alasan.empty())
            throw std::runtime_error("WAL tidak dipasang: " + alasan +
                                     ". Pindahkan DB ke disk lokal di luar folder sinkronisasi "
                                     "(atau pakai --paksa kalau kamu yakin).");
    }
    if (!o.no_backup) buat_backup(db, o.db, o.simpan_backup);
    aktifkan_wal(db, o.db, true);  // lokasi sudah dicek di atas (atau dipaksa)
    std::cout << "[OK] journal_mode=WAL aktif (disimpan di file DB, berlaku untuk semua koneksi)\n"
              << "     synchronous=NORMAL dipasang tiap rag membuka DB ini\n"
              << "     file -wal dan -shm muncul di samping DB selama ada koneksi yang terbuka\n";
}

void lepas(sqlite3* db, const Opsi& o) {
    const std::string mode = huruf_kecil(teks_pragma(db, "PRAGMA journal_mode"));
    if (mode != "wal") {
        std::cout << "[OK] DB tidak sedang WAL (mode: " << mode << "), tidak ada yang diubah\n";
        return;
    }
    if (!o.no_backup) buat_backup(db, o.db, o.simpan_backup);

    // PRAGMA journal_mode=DELETE butuh akses eksklusif. Kalau ada koneksi lain yang masih
    // terbuka, SQLite tidak melempar error: mode tetap 'wal'. Jadi hasilnya WAJIB dicek.
    auto st = siapkan(db, "PRAGMA journal_mode=DELETE");
    const int rc = sqlite3_step(st.get());
    if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED)
        throw std::runtime_error("gagal kembali ke DELETE: DB sedang dipakai program lain (database is locked). "
                                 "Tutup notebook/aplikasi yang membukanya lalu coba lagi. "
                                 "DB tetap WAL dan datanya tidak berubah.");
    if (rc != SQLITE_ROW) gagal(db, "journal_mode");
    const std::string baru = huruf_kecil(kolom_teks(st.get(), 0));
    st.reset();
    if (baru != "delete")
        throw std::runtime_error("gagal kembali ke DELETE (mode tetap '" + baru +
                                 "'). Tutup program lain yang memakai DB ini lalu coba lagi.");

    // buka_db memasang NORMAL karena DB tadi WAL. NORMAL tidak aman di mode DELETE.
    jalankan(db, "PRAGMA synchronous=FULL");
    std::cout << "[OK] journal_mode=DELETE; synchronous=FULL (file -wal/-shm sudah dihapus SQLite)\n";
}

}  // namespace

void perintah_wal(sqlite3* db, const Opsi& o) {
    const std::string sub = o.arg(1);
    if (sub.empty()) tampil_status(db, o);
    else if (sub == "pasang") pasang(db, o);
    else if (sub == "lepas") lepas(db, o);
    else throw std::runtime_error("aksi wal tidak dikenal: " + sub);
}
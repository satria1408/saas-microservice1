#include "meta_ttl.h"

#include <iostream>
#include <stdexcept>
#include <vector>

#include "meta_create.h"

static std::string bagian(const char* status, int hari) {
    return std::string("(COALESCE(status,'baru')='") + status +
           "' AND waktu_masuk < datetime('now','-" + std::to_string(hari) + " days'))";
}

void meta_pasang_ttl(sqlite3* db, int hari_ditolak, int hari_dipromosikan, int hari_baru) {
    for (int h : {hari_ditolak, hari_dipromosikan, hari_baru})
        if (h < 0 || h > 36500) throw std::runtime_error("hari harus 0..36500 (0 = tidak dibuang)");

    std::vector<std::string> aturan;
    if (hari_ditolak > 0) aturan.push_back(bagian("ditolak", hari_ditolak));
    if (hari_dipromosikan > 0) aturan.push_back(bagian("dipromosikan", hari_dipromosikan));
    if (hari_baru > 0) aturan.push_back(bagian("baru", hari_baru));
    if (aturan.empty()) throw std::runtime_error("semua nilai 0: tidak ada yang dipasang (pakai: cache ttl lepas)");

    std::string kondisi;
    for (size_t i = 0; i < aturan.size(); ++i) kondisi += (i ? " OR " : "") + aturan[i];

    jalankan(db, "BEGIN");
    try {
        // Baris lama dari notebook tidak punya waktu_masuk. Tanpa ini mereka tidak
        // pernah kedaluwarsa. Dimulai dari sekarang, jadi tidak ada yang langsung terhapus.
        jalankan(db, "UPDATE cache_metadata SET waktu_masuk=datetime('now') WHERE waktu_masuk IS NULL");
        jalankan(db, "CREATE INDEX IF NOT EXISTS idx_cache_meta_ttl ON cache_metadata(status, waktu_masuk)");

        // Penulis yang tidak mengisi waktu_masuk (notebook: INSERT 4 kolom) tetap kebagian.
        jalankan(db,
            "CREATE TRIGGER IF NOT EXISTS cache_meta_isi_waktu AFTER INSERT ON cache_metadata "
            "WHEN NEW.waktu_masuk IS NULL BEGIN "
            "UPDATE cache_metadata SET waktu_masuk=datetime('now') WHERE rowid=NEW.rowid; END");

        jalankan(db, "DROP TRIGGER IF EXISTS cache_meta_ttl");
        jalankan(db, ("CREATE TRIGGER cache_meta_ttl AFTER INSERT ON cache_metadata BEGIN "
                      "DELETE FROM cache_metadata WHERE waktu_masuk IS NOT NULL AND (" +
                      kondisi + "); END").c_str());
        jalankan(db, "COMMIT");
    } catch (...) {
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
    std::cout << "TTL dipasang: ditolak=" << hari_ditolak << " hari, dipromosikan="
              << hari_dipromosikan << " hari, baru=" << hari_baru
              << " hari (0 = tidak dibuang). 'ditinjau' tidak pernah dibuang.\n";
}

void meta_lepas_ttl(sqlite3* db) {
    jalankan(db, "DROP TRIGGER IF EXISTS cache_meta_ttl");
    std::cout << "Trigger TTL dicabut. Tidak ada lagi pembuangan otomatis.\n";
}

void meta_status_ttl(sqlite3* db) {
    {
        auto st = siapkan(db,
            "SELECT sql FROM sqlite_master WHERE type='trigger' AND name='cache_meta_ttl'");
        if (sqlite3_step(st.get()) == SQLITE_ROW)
            std::cout << "TTL: AKTIF\n  " << kolom_teks(st.get(), 0) << "\n";
        else
            std::cout << "TTL: tidak terpasang (pasang: rag cache ttl pasang)\n";
    }
    auto st = siapkan(db,
        "SELECT COALESCE(status,'baru'), COUNT(*), SUM(waktu_masuk IS NULL)"
        " FROM cache_metadata GROUP BY 1 ORDER BY 1");
    std::cout << "\nstatus | baris | tanpa waktu_masuk\n";
    int rc;
    while ((rc = sqlite3_step(st.get())) == SQLITE_ROW)
        std::cout << kolom_teks(st.get(), 0) << " | " << sqlite3_column_int64(st.get(), 1)
                  << " | " << sqlite3_column_int64(st.get(), 2) << "\n";
    if (rc != SQLITE_DONE) gagal(db, "membaca status ttl");
}
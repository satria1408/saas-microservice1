#include "meta_promosi.h"

#include <iostream>
#include <optional>
#include <stdexcept>

#include "crud/create.h"  
#include "util.h"

void meta_promosi(sqlite3* db, const std::string& key,
                  const std::string& penerbit_baru, const std::string& isbn_baru) {
    jalankan(db, "BEGIN");
    try {
        // 1. Ambil entri cache
        std::string judul, penulis, penerbit, status;
        std::optional<std::string> isbn_cache;
        {
            auto st = siapkan(db,
                "SELECT judul_asli, penulis_asli, penerbit, isbn, COALESCE(status,'baru')"
                " FROM cache_metadata WHERE judul_penulis_key=?");
            bind_teks(st.get(), 1, key);
            if (sqlite3_step(st.get()) != SQLITE_ROW)
                throw std::runtime_error("key '" + key + "' tidak ada di cache");
            if (sqlite3_column_type(st.get(), 0) == SQLITE_NULL)
                throw std::runtime_error(
                    "judul_asli kosong (baris lama dari notebook). "
                    "Tambahkan manual ke rag_manual dengan: rag add");
            judul = kolom_teks(st.get(), 0);
            if (sqlite3_column_type(st.get(), 1) != SQLITE_NULL) penulis = kolom_teks(st.get(), 1);
            if (sqlite3_column_type(st.get(), 2) != SQLITE_NULL) penerbit = kolom_teks(st.get(), 2);
            if (sqlite3_column_type(st.get(), 3) != SQLITE_NULL) isbn_cache = kolom_teks(st.get(), 3);
            status = kolom_teks(st.get(), 4);
        }
        if (status == "dipromosikan") {
            std::cout << "Sudah dipromosikan sebelumnya.\n";
            jalankan(db, "ROLLBACK");
            return;
        }
        if (status == "ditolak")
            throw std::runtime_error("entri berstatus 'ditolak'. Ubah dulu: rag cache status <key> ditinjau");

        // 2. Tentukan nilai final (argumen menimpa cache)
        std::string penerbit_final = trim(penerbit_baru).empty() ? penerbit : trim(penerbit_baru);
        std::string isbn_final = trim(isbn_baru).empty() ? (isbn_cache ? *isbn_cache : "")
                                                         : trim(isbn_baru);
        if (trim(penerbit_final).empty())
            throw std::runtime_error("tidak ada penerbit untuk dipromosikan, berikan lewat argumen");

        // 3. Masuk ke rag_manual (upsert by judul+penulis, ISBN divalidasi checksum)
        {
            Penyimpan p(db);
            p.simpan(judul, penulis, penerbit_final, isbn_final, "promosi_cache", true);
        }

        // 4. Samakan baris cache dengan hasil promosi. PENTING: urutan cek di notebook
        //    adalah cache -> rag_manual, jadi cache harus berisi nilai yang benar,
        //    kalau tidak nilai lama yang salah tetap menang.
        std::optional<std::string> isbn_valid = bersihkan_isbn(isbn_final);
        if (isbn_valid && !ean13_valid(*isbn_valid)) isbn_valid.reset();
        {
            auto upd = siapkan(db,
                "UPDATE cache_metadata SET penerbit=?, isbn=COALESCE(?,isbn),"
                " sumber='rag_manual', status='dipromosikan' WHERE judul_penulis_key=?");
            bind_teks(upd.get(), 1, penerbit_final);
            bind_opsional(upd.get(), 2, isbn_valid);
            bind_teks(upd.get(), 3, key);
            if (sqlite3_step(upd.get()) != SQLITE_DONE) gagal(db, "update cache");
        }

        jalankan(db, "COMMIT");
        std::cout << "Dipromosikan: '" << judul << "' -> rag_manual (penerbit: "
                  << penerbit_final << ")\n";
    } catch (...) {
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        throw; 
    }
}
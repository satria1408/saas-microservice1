# rag — struktur dan isi file

Dokumen ini menjelaskan **semua file** di folder `rag` dan bagaimana mereka bekerja sama.
Riwayat perubahan ada di [CHANGELOG.md](CHANGELOG.md). Gambaran proyek, pencapaian, dan cara
pakai ada di [README.md](../README.md).

## Ringkasan jumlah

Total **75 file**: 4 di folder `rag` dan 69 di `src/`.

| Jenis | Jumlah |
|---|---|
| `.cpp` | 34 |
| `.h` | 34 |
| `src/cache/base.none` | 1 (file kosong, 0 baris; tujuannya tidak tercatat) |
| Di luar `src`: `Makefile`, `build.bat`, `rag.exe`, `uji_tes.db` | 4 |

34 `.cpp` dan 34 `.h` terdiri dari **33 pasang** `.cpp` + `.h`, ditambah `main.cpp` (tanpa
header) dan `scan_model.h` (hanya header). Hitungan ini dari struktur zip. File buatan lokal
seperti `umur.py` dan backup `*.bak-*` tidak dihitung. Perubahan terbaru mengubah isi file yang
sudah ada dan tidak menambah file baru.

## Cara kerja

```
main.cpp -> baca_opsi -> validasi_bentuk -> buka_db -> jalankan_middleware
         -> router -> kontrol -> modul domain
```

1. **`validasi_bentuk`** menolak masukan yang salah **sebelum DB dibuka**: nama perintah,
   argumen wajib, `<id>` (angka, maksimal 18 digit), file CSV yang tidak bisa dibuka, ISBN, dan
   kombinasi opsi `--otomatis` / `--cek`.
2. **Middleware** menggolongkan perintah:
   - **baca-saja** (`list`, `cari`, `cari-isbn`, `katalog list/get`, `cache list`, `cache ttl`
     tanpa aksi, `cache scan list/get/isbn`, `cache promosi --otomatis --cek`) berjalan dengan
     `PRAGMA query_only = ON`;
   - **menulis** membuat backup lebih dulu, lalu memastikan skema;
   - **`cache promosi --otomatis`** dan **`cache bersihkan`** mengatur backup sendiri (tepat
     sebelum tulis/hapus pertama, tidak dibuat kalau tidak ada yang berubah) dan tidak mengubah
     skema diam-diam. Kalau skema belum mutakhir, jalankan `rag migrasi`.
3. Kesalahan dilempar sebagai `std::runtime_error`. `main` mencetak `[ERROR] <pesan>` dan keluar
   dengan kode 1.

### Backup
- Nama: `<db>.bak-YYYYMMDD-HHMMSS`. Dua backup dalam detik yang sama diberi akhiran `-02`,
  `-03`, dan seterusnya. Backup yang sudah ada tidak pernah ditimpa. Kalau sudah ada 99 backup
  pada detik yang sama, perintah berhenti dengan pesan jelas.
- Rotasi menyimpan `--simpan-backup N` backup terbaru (bawaan 5). Hanya nama yang persis
  berformat di atas yang dihitung dan dibuang.
- Backup adalah salinan file mentah. Hentikan proses lain yang sedang menulis ke DB yang sama
  sebelum menjalankan perintah tulis.

### TTL (ringkas)
Aturan umur untuk `cache_metadata`, `cache_scan`, dan `katalog` tersimpan di tabel `ttl_aturan`
dan dipakai sama oleh trigger dan `rag cache bersihkan`. Rinciannya ada di README (bagian Engine
C++) dan CHANGELOG.

## Daftar file

### Di luar `src`

| File | Kegunaan |
|---|---|
| `Makefile` | build lewat `make`, mengumpulkan semua `.cpp` dengan wildcard |
| `build.bat` | build di Windows, daftar file ditulis satu per satu (tambah di sini kalau ada `.cpp` baru) |
| `rag.exe` | hasil build (binary), bukan kode sumber; jangan di-commit |
| `uji_tes.db` | database contoh untuk uji: 200 buku di `rag_manual` plus sedikit cache dan katalog |
| 'tes_rag.py' | untuk fitur testing via cli agar tidak menulis cmd satu persatu secara manual |
| 'umur.py' | untuk fitur menghitung waktu, hari, minggu, bulan, tahun |

### Inti (`src/`)

| File | Kegunaan |
|---|---|
| `main.cpp` | pintu masuk: baca opsi, validasi, buka DB, middleware, router. Menangkap error menjadi `[ERROR] ...` dengan exit code 1 |
| `db.h/.cpp` | pembungkus SQLite: buka DB, prepared statement, bind, `buat_backup` (rotasi, akhiran anti-tabrakan), skema `rag_manual` |
| `util.h/.cpp` | `ean13_valid`, `isbn13_valid` (978/979 + checksum), `bersihkan_isbn` (membuang `-` dan semua whitespace), `trim`, `huruf_kecil`, `batas_atas_awalan` |
| `csv.h/.cpp` | pembaca CSV (pemisah koma, kutipan) |

### `src/cli/` (jalur perintah)

| File | Kegunaan |
|---|---|
| `opsi.h/.cpp` | struct `Opsi` dan pembaca argumen/opsi (`--db`, `--hari`, `--simpan-backup`, `--no-backup`, `--tanpa-transaksi`, `--rinci`, `-y`, `--otomatis`, `--cek`), plus `baca_angka`, `baca_id`, `wajib` |
| `validasi.h/.cpp` | memeriksa bentuk perintah dan argumen sebelum DB dibuka |
| `middleware.h/.cpp` | menggolongkan perintah (baca/tulis), membuat backup, memastikan skema, `query_only` untuk perintah baca |
| `router.h/.cpp` | mengarahkan perintah ke kontrol yang sesuai |
| `bantuan.h/.cpp` | teks bantuan (`rag` tanpa argumen) |
| `kontrol/rag_kontrol` | `add`, `import`, `list`, `edit`, `hapus` |
| `kontrol/cache_kontrol` | semua `rag cache ...` (list, add, status, hapus, promosi, bersihkan, ttl) |
| `kontrol/scan_kontrol` | `rag cache scan ...` |
| `kontrol/katalog_kontrol` | `rag katalog ...` |
| `kontrol/cari_kontrol` | `cari` dan `cari-isbn` |
| `kontrol/migrasi_kontrol` | `migrasi` (backup dulu, lalu mutakhirkan skema semua tabel) |

### `src/crud/` (tabel `rag_manual`)

| File | Kegunaan |
|---|---|
| `create.h/.cpp` | `Penyimpan` (upsert berdasarkan judul+penulis), `tambah`, `impor` CSV |
| `read.h/.cpp` | `daftar` dengan filter kata kunci |
| `update.h/.cpp` | `edit` satu kolom |
| `delete.h/.cpp` | `hapus` berdasarkan id |

### `src/cache/metadata/` (tabel `cache_metadata`)

| File | Kegunaan |
|---|---|
| `meta_create` | `buat_key`, skema dan trigger pelindung, `meta_simpan` dan `cache add` |
| `meta_read` | `cache list` |
| `meta_kelola` | ubah status, hapus satu entri |
| `meta_promosi` | promosi manual (boleh menimpa) dan otomatis (tidak pernah menimpa) ke `rag_manual` |
| `meta_ttl` | TTL tiga tabel: pasang, lepas, status, bersihkan (satu aturan di tabel `ttl_aturan`) |

### `src/cache/scan/` (tabel `cache_scan`)

| File | Kegunaan |
|---|---|
| `scan_skema` | membuat tabel dan kolom (`kategori`, `waktu_masuk`, `isbn`) |
| `scan_model.h` | struct `ScanCache` |
| `scan_create` | `cache scan add` (upsert; ISBN-13 divalidasi; `waktu_masuk` tidak direset saat hash sama discan ulang) |
| `scan_read` | `list`, `get`, `isbn` |
| `scan_hapus` | hapus berdasarkan awalan hash |
| `scan_bersihkan` | pembersihan manual `--hari N` untuk `cache_scan` saja (pembersihan berbasis aturan lewat `cache bersihkan`) |

### `src/katalog/` dan `src/alur/`

| File | Kegunaan |
|---|---|
| `katalog_skema` | membuat tabel `katalog` |
| `katalog_read` | `katalog list` dan `get` |
| `katalog_ubah` | `katalog konfirmasi` (penerbit, ISBN, stok) |
| `alur_cari` | pencarian lokal beserta jejaknya: cache, katalog, lalu `rag_manual` |
| `kemiripan` | rasio kemiripan gaya `difflib` untuk pencocokan judul (ambang 0,75) |

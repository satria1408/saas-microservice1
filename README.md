# Book Scanner — Progress & Roadmap

## Progress Sesi Sebelumnya (Jumat)

**Arsitektur inti:**
- Scan cover (Qwen2-VL) → judul, penulis, kategori. Prompt versi longgar (instruksi positif, boleh jawab null) terbukti lebih akurat dari versi ketat.
- Penerbit tidak pernah dari tebakan Qwen — selalu dari cache, RAG manual, Open Library (`editions.json`, difilter bahasa eng/ind), atau lookup ISBN langsung. Kalau tidak ketemu, `null` (dianggap benar, bukan gagal).
- Kategori dinormalisasi otomatis ke daftar baku, toleran ke variasi format/typo ringan.
- Cache 2 arah: `cache_scan` (by hash gambar) dan `cache_metadata` (by judul+penulis). ISBN dari input manual otomatis menular ke hasil scan cover berikutnya.
- Jalur ISBN langsung (`cari_dari_isbn`) dengan cache tersendiri.
- Katalog SQLite: hasil scan langsung tersimpan otomatis, dengan dedup — buku yang sama menambah stok, bukan menumpuk baris baru.
- Normalisasi pencocokan buku tahan aksen dan variasi nama penerbit.
- Live API (FastAPI + ngrok) stabil: fix asyncio Python 3.13, fix koneksi SQLite closed, resize+convert JPG di endpoint `/scan` untuk mencegah OOM.
- Migrasi skema tabel terpusat lewat `DAFTAR_MIGRASI`.

**Fitur RAG manual:**
- Tabel `rag_manual` (SQLite), pencarian pakai fuzzy string matching (`difflib`).
- Urutan pengecekan di `lengkapi_metadata` (jalur scan cover): cache → RAG manual → Open Library → null. RAG manual jadi override, bukan sekadar fallback, karena kasus "salah edisi" (Open Library mencocokkan ke edisi terjemahan/berbeda) lebih sering terjadi daripada kasus "tidak ketemu sama sekali".
- Basis data diperluas ke 175 judul lalu dipangkas ke 100 judul terverifikasi. Kolom ISBN ditambahkan ke `rag_manual`.
- Endpoint `/isbn/{isbn}` diberi fallback ke RAG manual (exact match by ISBN) untuk buku lokal/indie yang tidak terdaftar di Open Library.

**Bug besar yang ditemukan & diperbaiki:**
- `search.json` Open Library mengembalikan field `publisher` sebagai gabungan seluruh penerbit dari seluruh edisi (bisa 100+ nama campur bahasa) — diperbaiki dengan `editions.json` + filter bahasa.
- Google Books API dihapus dari pipeline (kena limit kuota harian publik).
- Beberapa kali ditemukan sisa kode versi lama yang tidak terhapus penuh saat revisi fungsi, menyebabkan hasil tertimpa kembali — solusinya selalu replace seluruh fungsi, bukan sisipkan potongan.
- Verifikasi kasus "salah edisi": Cantik Itu Luka, El Principito, Fahrenheit 451 — Open Library sempat mengarah ke edisi/cetakan berbeda dari fisik buku yang di-scan.

## Progress Hari Ini — Fitur Scan Barcode ISBN

**Masalah yang diselesaikan:** input ISBN manual (13 digit ketik tangan) rawan salah pencet, menyebabkan ISBN nyasar ke buku lain atau tidak ketemu tanpa jelas letak salahnya.

**Arsitektur:**
```
Foto barcode
    |
Baca file lewat PIL + exif_transpose (bukan cv2.imread langsung)
    |
Coba decode:
  1. OpenCV (cv2.barcode.BarcodeDetector) — dicoba duluan, sudah tersedia
     bawaan Colab (OpenCV 5.0.0), tanpa install tambahan
  2. zxing-cpp — fallback kalau OpenCV nihil
    |
Setiap kandidat divalidasi checksum EAN-13 (13 digit + prefix 978/979 +
checksum matematis) — bukan berdasarkan nama format dari library
    |
ISBN valid → cari_dari_isbn(isbn), fungsi yang SAMA dengan jalur ISBN manual
    |
cache_metadata → Open Library → rag_manual → null
```

Qwen tidak dilibatkan sama sekali di jalur ini — scan barcode dan scan cover (untuk judul/penulis/kategori) adalah dua jalur input independen yang bermuara ke tabel `katalog` yang sama.

**Keputusan desain penting:**
1. Validasi pakai checksum matematis EAN-13, bukan pencocokan string nama format dari library. Penamaan format ternyata tidak konsisten antar versi/library, dan sempat meloloskan kasus nyata: foto berisi QR code tidak terkait (link YouTube) hampir masuk ke `cari_dari_isbn` sebelum filter diganti. Sejak pindah ke validasi checksum, nol false-positive di semua kasus uji.
2. OpenCV dicoba lebih dulu (lebih cepat, sudah tersedia), zxing-cpp sebagai fallback kedua.
3. Baca file pakai PIL, bukan `cv2.imread()` langsung — ditemukan `cv2.imread()` gagal diam-diam (mengembalikan `None` tanpa error) untuk file `.webp` pada build OpenCV yang tidak menyertakan `libwebp`. `ImageOps.exif_transpose()` sekaligus membetulkan orientasi foto HP yang sering "diputar" lewat tag EXIF.
4. Fungsi barcode hanya menghasilkan string ISBN lalu memanggil `cari_dari_isbn()` yang sudah ada — logika lookup tidak diduplikasi.
5. **Urutan prioritas Open Library vs rag_manual berbeda per jalur, dan ini disengaja:** jalur ISBN/barcode (nilai ISBN sudah pasti) mencoba Open Library dulu, rag_manual sebagai fallback. Jalur scan cover (judul+penulis dicocokkan, rawan salah edisi) memprioritaskan rag_manual di atas Open Library. Ini bukan inkonsistensi — tingkat kepastian input di kedua jalur berbeda.

**Hasil uji (kualitatif):**
- Barcode difoto dekat (5-10 cm) dan lurus: selalu terbaca konsisten.
- Blur, jarak jauh, sudut miring, atau pantulan plastik pembungkus: gagal di OpenCV **dan** zxing-cpp sekaligus — disimpulkan sebagai batas fisik pembacaan barcode lewat kamera HP, bukan kelemahan satu library yang bisa ditambal library lain.
- Kode non-ISBN (barcode harga toko, QR tak terkait) ditolak dengan benar setelah perbaikan filter checksum.
- Uji sudah mencakup jalur penuh lewat HTTP (endpoint `/scan-barcode`), bukan hanya pemanggilan fungsi langsung.
- Belum dilakukan: pengujian kuantitatif berskala (N foto, % berhasil, ground truth tercatat) — uji sejauh ini bersifat kasus-per-kasus untuk memvalidasi desain.

**Keterbatasan yang diketahui:**
1. Heuristik crop rasio-tetap hanya cocok untuk foto yang cover-nya memenuhi bingkai; foto candid/miring bisa membuat barcode jatuh di luar area yang ditebak, bahkan `detect()` OpenCV gagal menemukan lokasinya.
2. Belum ada fallback OCR untuk baris teks "ISBN ..." tercetak — beberapa foto yang barcode-nya gagal dibaca punya teks ISBN yang masih terbaca jelas oleh mata manusia.
3. `rag_manual` baru ~100 entri; untuk buku yang tidak ada di Open Library *dan* tidak ada di rag_manual, hasilnya tetap `isbn_tidak_ketemu`.
4. Perilaku stok belum konsisten antar jalur (scan foto selalu +1, sebagian jalur ISBN tidak) — bukan bug baru dari fitur ini, melainkan perilaku yang diwarisi dari `cari_dari_isbn`.
5. Field kategori tidak terisi dari jalur ISBN/barcode, karena sumber kategori sejauh ini hanya dari hasil scan Qwen di foto cover depan.

## Isu Terbuka

- `NameError` pada fungsi-fungsi kunci (`cari_dari_isbn`, dll) beberapa kali muncul setelah restart runtime karena urutan eksekusi sel tidak dari awal — perlu kebiasaan `Runtime → Run all` setiap sesi baru, dengan sel Live API diletakkan paling akhir.
- Perilaku stok antar jalur (scan cover vs ISBN manual vs barcode) belum diseragamkan.

## Rencana Jangka Menengah/Panjang

Belum diimplementasikan, masih berupa daftar terbuka:

1. Perluasan RAG manual — memprioritaskan buku pelajaran SD/SMP/SMA dan buku keterampilan (DIY/prakarya), yang diperkirakan volumenya tinggi di perpustakaan sekolah namun jarang terindeks di Open Library.
2. Deteksi lokasi barcode yang lebih aktif (mis. sliding window atau localisation yang lebih toleran terhadap foto miring), menggantikan heuristik crop rasio-tetap saat ini.
3. Fallback OCR untuk teks "ISBN ..." sebagai jaring pengaman terakhir sebelum menyerah ke input manual.
4. Endpoint konfirmasi/tambah-ke-rag_manual langsung dari hasil `isbn_tidak_ketemu`, supaya data rag_manual tumbuh dari pemakaian nyata, bukan hanya input manual di notebook.
5. Penyeragaman perilaku stok di semua jalur input (scan, ISBN manual, barcode).
6. Semantic search berbasis embedding untuk pencarian katalog berbasis makna/tema — membutuhkan data tambahan berupa sinopsis singkat per buku (bukan isi buku penuh, untuk menghindari isu hak cipta).
7. Test case tertulis untuk mendeteksi regresi saat kode berubah, termasuk evaluasi kuantitatif fitur barcode dengan sampel lebih besar.
8. Rate limit & autentikasi yang lebih ketat di Live API.
9. Evaluasi arsitektur deployment produksi (Colab + ngrok belum cocok untuk operasional 24/7; opsi GPU cloud perlu estimasi biaya dan model penjadwalan sebelum digunakan produksi).

## Dependensi

```
pip install zxing-cpp
```
## Progress Hari Ini — CRUD `rag_manual` (C++) & Cache Metadata

**Tujuan:** data kurasi tidak lagi terkunci di runtime Colab. Menambah atau mengoreksi buku cukup lewat CLI lokal `rag.exe`, tanpa menyalakan sesi Colab. Colab diposisikan sebagai mesin inferensi Qwen saja.

**Struktur `rag/`:**

```
rag/
├─ Makefile / build.bat
└─ src/
   ├─ main.cpp                 ← parse opsi + dispatch
   ├─ db.h/.cpp                ← RAII SQLite, prepare/bind, skema, backup
   ├─ util.h/.cpp              ← checksum EAN-13, trim, huruf kecil
   ├─ csv.h/.cpp               ← parser CSV (kutip, CRLF, BOM)
   ├─ crud/                    ← rag_manual: create / read / update / delete
   └─ cache/metadata/          ← cache_metadata
      ├─ meta_create.*         ← skema, buat_key, meta_simpan
      ├─ meta_read.*           ← list + filter status/kata
      ├─ meta_kelola.*         ← ubah status, hapus per key, bersihkan massal
      └─ meta_promosi.*        ← cache -> rag_manual (1 transaksi)
```

**Perintah CLI** (`--db path` atau env `RAG_DB`, default `book_cache.db`):

```
rag add <judul> <penulis> <penerbit> [isbn]
rag import file.csv [--tanpa-transaksi] [--rinci]
rag list [kata]
rag edit <id> <judul|penulis|penerbit|isbn> <nilai>
rag hapus <id> [-y]

rag cache list [baru|ditinjau|dipromosikan|ditolak] [kata]
rag cache status <key> <baru|ditinjau|ditolak>
rag cache hapus <key> [-y]
rag cache bersihkan [--hari N] [-y]
rag cache promosi <key> [penerbit] [isbn]
rag cache add <judul> <penulis> <penerbit> [isbn]

Opsi: --no-backup
```

**Konsep cache sementara:** `cache_metadata` adalah tempat singgah hasil lookup (misalnya dari Open Library) sebelum diverifikasi, bukan cache yang kedaluwarsa sendiri. Alurnya:

```
Open Library / scan -> cache_metadata (baru) -> tinjau -> promosi -> rag_manual (terverifikasi, override)
```

**Kolom baru di `cache_metadata`** (migrasi otomatis, aman untuk DB lama dari notebook): `judul_asli`, `penulis_asli`, `status`, `waktu_masuk`. `judul_asli` dan `penulis_asli` dibutuhkan karena `judul_penulis_key` sudah dinormalisasi dan teks aslinya tidak bisa dipulihkan.

**Keputusan desain penting:**

1. **Promosi atomik.** Salin ke `rag_manual` dan update baris cache terjadi dalam satu transaksi. Gagal di langkah mana pun berarti ROLLBACK, tidak ada kondisi "sudah masuk RAG tapi masih berstatus baru".
2. **Promosi juga menimpa baris cache** dengan nilai yang benar. Urutan cek di notebook adalah cache -> `rag_manual` -> Open Library, jadi kalau cache dibiarkan berisi penerbit salah edisi, koreksi di `rag_manual` tidak pernah terbaca.
3. **Koreksi salah edisi lewat argumen promosi** (`cache promosi <key> "Penerbit Benar" <isbn>`), sebagai pengganti fitur edit isi cache yang sengaja dibuang. Cache ditulis otomatis oleh proses scan, jadi yang perlu manusia hanya meninjau, memutuskan, dan membersihkan.
4. **Status `dipromosikan` tidak bisa diatur manual**, hanya berubah lewat promosi yang benar-benar menyalin data.
5. **`bersihkan` tidak pernah menghapus entri `ditinjau`.** Tanpa `--hari`, hanya `dipromosikan` dan `ditolak` yang dihapus.
6. **`buat_key` meniru `_normalisasi_key` di notebook** supaya baris yang ditulis C++ dan Python punya kunci yang sama.
7. **Backup otomatis** (`<db>.bak-<waktu>`) sebelum perintah yang menulis, dibuat sebelum migrasi skema.
8. **`cache_metadata` dan `rag_manual` tetap satu file DB**, supaya promosi bisa atomik.

**Build (Windows, MSYS2 CLANG64):**

```powershell
C:\msys64\usr\bin\pacman.exe -S --needed mingw-w64-clang-x86_64-sqlite3
$env:Path = "C:\msys64\clang64\bin;" + $env:Path
.\build.bat
```

`build.bat` memakai daftar file eksplisit dan perlu diedit tiap ada file `.cpp` baru. Makefile memakai wildcard tiga level tapi butuh shell yang punya `mkdir -p` dan `rm` (terminal MSYS2, bukan PowerShell). Flag `-Isrc` wajib.

**Hasil uji:**

- Uji otomatis di sandbox (g++, Linux, skema DB ditiru dari notebook): build 12 objek tanpa warning `-Wall -Wextra`; migrasi skema; alur add -> status -> promosi -> bersihkan; promosi ulang idempoten; entri `ditolak` tidak bisa dipromosikan; ROLLBACK terbukti dengan menggagalkan UPDATE cache lewat trigger; `buat_key` cocok dengan normalisasi Python pada 12 judul sulit (tanda baca, aksen, CJK, penulis kosong), 0 beda.
- Uji manual di mesin sendiri (clang64): build bersih setelah dua titik nyasar di `meta_kelola.h` dan `meta_create.cpp` dibuang; migrasi pada salinan `book_cache.db` asli; promosi satu entri sampai berstatus `dipromosikan` dengan sumber `rag_manual`.
- **Belum diuji:** perilaku `bersihkan --hari` pada baris lama notebook yang `waktu_masuk`-nya kosong (dari membaca kode, baris itu tidak pernah lolos kriteria umur); perilaku dengan clang/libc++ di luar mesin ini; sisi Python.

**Keterbatasan yang diketahui:**

1. **Baris lama dari notebook tidak bisa langsung dipromosikan**, karena `judul_asli` kosong. Jalurnya: `cache add` ulang judul dan penulis aslinya (status tidak tersentuh), lalu `cache promosi`.
2. **ISBN override tidak valid di `promosi`** hanya memunculkan `[WARN]` dan promosi lanjut dengan ISBN kosong, bukan dibatalkan. Keputusannya belum diambil.
3. **Notebook memakai `INSERT OR REPLACE` ke `cache_metadata`**, yang mengembalikan `judul_asli`, `penulis_asli`, dan `status` ke default. Harus diganti upsert sebelum notebook dan CLI menulis ke tabel yang sama.
4. **`cache list` diurutkan `rowid`** (urutan masuk), jadi entri yang sudah dipromosikan tidak naik ke atas.
5. **`rag.exe` di luar terminal MSYS2** butuh DLL dari `clang64\bin` di PATH.
6. **Proyek ada di folder OneDrive.** Sinkronisasi bisa mengunci atau menduplikasi file `.db` dan `.bak-*`.
7. Belum ada `busy_timeout` atau WAL, jadi dua penulis bersamaan ke satu file bisa menghasilkan `database is locked`. Belum ada test tertulis.

**Isu terbuka:**

- Keputusan perilaku ISBN override tidak valid (batalkan promosi atau lanjut dengan ISBN kosong).
- Urutan tampil `cache list` (antrean kerja di atas, yang selesai di bawah, atau sebaliknya).

**Rencana berikutnya:**

1. Modul `cache/scan/` untuk `cache_scan` (sejajar `cache/metadata/`). Murni cache, tanpa jalur promosi.
2. Ganti `INSERT OR REPLACE` di notebook dengan upsert yang mempertahankan kolom baru.
3. Perbaiki bug `_normalisasi_kategori` di notebook (`non_fiksi` terbaca sebagai `fiksi`).
4. `.gitignore` untuk `build/`, `rag.exe`, `*.db`, dan `*.bak-*`. Pastikan `book_cache.db` dan API key di notebook tidak ikut ter-commit.
5. Test tertulis untuk parser CSV, upsert, dan promosi.
6. Pindahkan DB harian ke luar OneDrive.

# Changelog — engine C++ `rag`

Mencatat perubahan pada engine `rag/`. Perubahan di notebook dan FastAPI lokal ada di
[README.md](../README.md). Daftar file ada di [STRUKTUR.md](STRUKTUR.md).

Format: yang terbaru di atas. Belum ada nomor versi, jadi tiap bagian diberi tanggal.

## 2026-10-13

### Ditambahkan

- **Promosi otomatis:** `rag cache promosi --otomatis [--cek] [--rinci]` menaikkan entri
  `cache_metadata` yang lengkap ke `rag_manual`.
  - Syarat naik: status `baru` atau `ditinjau`; `judul_asli`, `penulis_asli`, dan penerbit
    terisi; ISBN lolos ISBN-13 (978/979 + checksum); dan bukunya belum ada di `rag_manual`
    (judul+penulis atau ISBN yang sama).
  - **Tidak pernah menimpa** buku yang sudah ada. Yang masuk duluan (`waktu_masuk` paling
    awal) dipakai; yang kembar dilewati. Promosi manual (`cache promosi <key>`) tetap boleh
    menimpa, untuk koreksi admin.
  - `--cek` hanya melaporkan: tidak menulis dan tidak membuat backup. `--rinci` menampilkan
    baris yang menunggu atau dilewati.
  - Satu transaksi per baris. Baris yang gagal di-rollback dan dilaporkan, baris lain tetap
    diproses, dan exit code menjadi 1.
- **Opsi baru** `--otomatis` dan `--cek`. Dipakai di luar `cache promosi`, atau `--cek` tanpa
  `--otomatis`, ditolak sebelum DB dibuka.
- <!-- Hapus butir ini kalau kode penyamaan di meta_promosi.cpp belum diterapkan. -->
  **Penyamaan baris yang kalah:** baris cache berstatus `baru` yang kalah dari `rag_manual`
  disamakan penerbit dan ISBN-nya dengan pemenangnya, supaya pencarian (cache dibaca lebih dulu)
  tidak menampilkan data yang kalah. Baris `ditinjau` tidak disentuh, dan status tetap `baru`.
- **TTL konsisten di tiga tabel** (`cache_metadata`, `cache_scan`, `katalog`):
  - Aturan tersimpan di tabel `ttl_aturan` dan dipakai sama oleh trigger (`cache_meta_ttl`,
    `scan_ttl`, `katalog_ttl`) dan oleh `rag cache bersihkan`.
  - Bawaan (hari): `cache_metadata` baru 90, ditolak 7, dipromosikan 0 (tidak dibuang);
    `cache_scan` 90; `katalog` otomatis 90. Tidak pernah dibuang: `cache_metadata` `ditinjau`
    dan `katalog` `terkonfirmasi` (berisi stok).
  - Umur dihitung dari `waktu_masuk` (kapan pertama masuk). Trigger pengisi `waktu_masuk`
    (`cache_meta_isi_waktu`, `scan_isi_waktu`, `katalog_isi_waktu`) menjaga penulis yang tidak
    mengisinya, seperti notebook.
  - `rag cache ttl` menampilkan trigger, aturan, dan jumlah baris yang sudah kedaluwarsa per
    kelas (hanya membaca).

### Diubah

- **`rag cache bersihkan`** sekarang menjalankan aturan TTL yang sama dengan trigger pada ketiga
  tabel. Sebelumnya ia menghapus **semua** entri `dipromosikan` dan `ditolak` tanpa melihat umur,
  berbeda dari trigger.
  - `--hari N` kini berarti "ganti umur untuk `baru`, `cache_scan`, dan `katalog` otomatis".
    Kelas lain tidak terpengaruh.
  - Jawab `n` pada pertanyaan konfirmasi: tidak ada backup, tidak ada yang dihapus.
  - Penghapusan ketiga tabel dalam satu transaksi.
- **`rag cache ttl pasang`** menerima 5 angka: `[ditolak] [dipromosikan] [baru] [scan] [katalog]`
  (bawaan `7 0 90 90 90`), sebelumnya 3. Baris lama tanpa `waktu_masuk` diisi dengan waktu
  sekarang agar mulai dihitung (tidak ada yang langsung terhapus). `rag cache ttl lepas`
  mencabut ketiga trigger.
- **Backup lebih hemat:** `cache promosi --otomatis` dan `cache bersihkan` membuat backup tepat
  sebelum tulis/hapus pertama, tidak sama sekali kalau tidak ada yang berubah. Keduanya juga
  tidak mengubah skema diam-diam (kalau belum mutakhir: error dengan saran `rag migrasi`).

### Diperbaiki

| Berkas | Sebelum | Sesudah |
|---|---|---|
| `src/util.cpp` | `bersihkan_isbn` hanya membuang `-` dan spasi biasa, jadi ISBN dengan tab/newline dikosongkan dengan `[WARN]` | membuang semua whitespace |
| `src/db.cpp` | dua perintah tulis dalam satu detik menghasilkan nama backup sama, backup kedua menimpa yang pertama | akhiran `-02`, `-03`, ... dan tidak pernah menimpa; rotasi menerima akhiran baru |
| `src/cli/validasi.cpp` | `rag import file_tidak_ada.csv` membuat backup (dan merotasi backup lama) lalu error | file dicek sebelum DB dibuka; tanpa backup |
| `src/cli/opsi.cpp` | `rag hapus 99999999999999999999` hanya mencetak `[ERROR] stoll` | `baca_id` membatasi 18 digit dengan pesan jelas (berlaku juga untuk `edit` dan `katalog get/konfirmasi`) |
| `src/cli/bantuan.cpp` | bantuan tertinggal dari kode | memuat `[isbn]`, `cache scan isbn`, `cache ttl`, `cache promosi --otomatis`, dan semua opsi |
| `src/cache/scan/scan_create.cpp` | `cache scan add` pada hash yang sama menimpa kategori dengan kosong dan mereset `waktu_masuk` | `judul`, `penulis`, `kategori`, `isbn` memakai `COALESCE`; `waktu_masuk` hanya diisi saat insert pertama |

### Dihapus

- `meta_bersihkan` di `meta_kelola.*`, digantikan `ttl_bersihkan` di `meta_ttl.*`.

### Catatan untuk DB yang sudah ada

- Jalankan `rag migrasi` sekali (ada backup otomatis) supaya `cache_scan` punya kolom `isbn`
  dan `waktu_masuk`.
- TTL **tidak aktif sendiri**. Pasang dengan `rag cache ttl pasang` (cek dulu dengan
  `rag cache ttl` berapa baris yang sudah kedaluwarsa).
- Trigger TTL hanya berjalan saat ada INSERT ke tabelnya. `rag cache bersihkan` menjalankannya
  sekarang.
- **Jangan menghapus tabel `ttl_aturan`** selagi trigger terpasang: INSERT ke tabel itu akan
  gagal dengan `no such table: main.ttl_aturan`. Gunakan `rag cache ttl lepas`.
- Baris `katalog` berstatus `otomatis` kini punya batas hidup 90 hari. Jembatan `katalog` ke
  `rag_manual` belum ada, jadi baris itu perlu dikonfirmasi (`rag katalog konfirmasi`) sebelum
  waktunya.

  ### Fitur wal
  - `BEGIN` biasa belum diganti `BEGIN IMMEDIATE` (`cache promosi`, `import`, promosi otomatis).
- Belum ada UNIQUE index / constraint di level DB, jadi duplikat masih bisa lolos.
- Tautan cache ke `rag_manual` masih lewat teks, bukan id.
- Skema belum berversi (`PRAGMA user_version`).
- `katalog konfirmasi` rawan lost-update.
- Trigger TTL menghapus baris lama diam-diam saat ada INSERT.


## Sebelum 2026-10-13

Kondisi pada zip awal: CRUD `rag_manual`, `cache_metadata` (promosi manual, status, trigger
pelindung, TTL satu tabel), `cache_scan`, `katalog`, pencarian lokal (`cari`, `cari-isbn`),
`migrasi`, backup berotasi, dan fuzzy match gaya `difflib`.

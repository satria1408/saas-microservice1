# Book Scanner — Progress & Roadmap

Terakhir diperbarui: 6 Oktober 2026

## Peta Proyek

Proyek ini terdiri dari beberapa bagian yang berjalan terpisah. Tabel ini memisahkan apa yang **sudah ada di repo** dari yang masih **rencana**.

| Bagian | Lokasi | Status |
|---|---|---|
| Notebook Colab: Qwen2-VL, cache, katalog, RAG manual, Live API (FastAPI + ngrok) | `scan_buku_qwen2vl_v3_cache_isbn.ipynb` | Jalan |
| FastAPI lokal: scan lewat Colab, katalog Python, rekomendasi TF-IDF | `main.py`, `config.py`, `routers/`, `services/`, `scripts/` | Jalan, dengan beberapa catatan (lihat bagian FastAPI Lokal) |
| Engine data C++ (`rag.exe`): CRUD `rag_manual`, cache, katalog, pencarian lokal | `rag/` | Jalan sebagai CLI. **Belum dipanggil dari Python** |
| `rag_cli.py`: alat Python untuk mengelola `rag_manual` | akar repo | Jalan, tumpang tindih dengan CRUD C++ |
| Django + Django Ninja (backend website) dan React (frontend) | belum ada | Rencana |

**Dua database SQLite yang berbeda saat ini:**

- `book_cache.db`: milik notebook dan engine C++ (`cache_scan`, `cache_metadata`, `katalog`, `rag_manual`).
- `data/book_catalog.db`: milik FastAPI lokal, tabel `books` (judul, penulis, penerbit, kategori, penjelasan), dipakai untuk rekomendasi.

Dua katalog ini belum didamaikan.

### Arsitektur target (rencana, belum ada kodenya di repo)

```
React (browser)
   |
Django + Django Ninja            <- satu-satunya pintu dari luar
   |
   +-- FastAPI notebook (Colab, via ngrok): Qwen2-VL saja, tidak menyimpan data
   |
   +-- FastAPI lokal --> engine data C++ --> SQLite
```

Aturan yang diusulkan untuk rancangan ini:

1. **React hanya bicara ke Django.** Kode React dikirim ke browser dan bisa dibaca siapa pun, jadi tidak boleh memegang API key atau alamat layanan internal.
2. **Semua penulisan data lewat engine C++** (satu penjaga untuk file SQLite). Aturan seperti promosi atomik, validasi ISBN, dan perlindungan baris yang sudah dipromosikan cukup ditulis sekali di C++.
3. **Colab stateless:** terima gambar, kembalikan hasil baca, selesai. Tidak ada cache atau katalog yang hidup di Colab.
4. **Akun dan password sebaiknya tetap di database Django** (hash dan sesi sudah ditangani Django), bukan di engine C++. Data aplikasi per pengguna merujuk akun lewat `user_id`.

Keputusan yang masih terbuka: Django dijalankan di mana (mesin yang sama atau server terpisah), apakah FastAPI lokal tetap perlu kalau Django ada di mesin yang sama, dan jembatan Python ke C++ (subprocess, `ctypes`, atau service HTTP lokal).

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

## Progress — Fitur Scan Barcode ISBN

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
- **Uji tambahan (sandbox g++ di Linux, skema DB ditiru dari notebook, bukan di clang64):**
  - Bug "entri `dipromosikan` ditimpa": `cache add` ulang mencetak `[lewati]` dan baris utuh. `INSERT OR REPLACE` 4 kolom dari Python ke key yang sama tidak mengubah baris. Kontrol: key biasa tetap bisa ditulis dan diganti.
  - ISBN koreksi tidak valid (checksum salah, atau huruf) di `promosi`: ditolak, `rag_manual` tidak berubah, cache tetap `baru`. ISBN valid bertanda hubung diterima.
  - Rotasi backup: dari 7 backup sah dengan batas 3, 4 yang tertua dibuang. Empat file umpan (milik DB lain, akhiran salah, kependekan, kelebihan `.txt`) tidak tersentuh. `--simpan-backup 0` dan `abc` ditolak.
  - TTL: baris kedaluwarsa dibuang saat INSERT dari penulis ala notebook, yang dilindungi (`ditinjau`, belum kedaluwarsa, `dipromosikan` dengan TTL 0) tetap ada, `waktu_masuk` terisi otomatis, `lepas` menghentikan pembuangan, argumen salah ditolak. Trigger pelindung dan trigger TTL tidak saling mengganggu.
  - **Fuzzy C++ vs `difflib` asli, 26.276 pasangan, 0 skor berbeda** pada: judul+penulis dengan typo (8.000), dua buku acak (8.000), ASCII acak dengan banyak seri (8.000), string 200 karakter ke atas (1.500), kosong dan 1-2 karakter (308), Latin-1 beraksen (144). Selisih hanya muncul untuk karakter di luar ASCII dan Latin-1 (lihat keterbatasan).
  - Seluruh `rag/` terkompilasi bersih dengan `-Wall -Wextra` (g++), dan semua `.cpp` terdaftar di `build.bat`.
- **Uji manual di mesin sendiri (clang64):** `rag cari "Student Hidjoo" "Mas Marco Kartodikromo"` menemukan `Student Hidjo` di `rag_manual` dengan skor 0,986, sama dengan `difflib` asli (0,986301). Jejak pencarian tampil lengkap.
- **Belum diuji:** `rag cari` dan `cari-isbn` end-to-end pada DB di sandbox (hanya fungsi kemiripan dan kompilasinya); `katalog list/get/konfirmasi`; `cache scan bersihkan`; transaksi baca-lalu-tulis di bawah beban penulis bersamaan; sisi Python memanggil C++.
- oke gua tadi udah bikin satu penghubung antara cache ada cache_metadata sama cache_scan agar buku memiliki identitas di cache scan wong cache scan cuma hash gambar. jadi agar buku yang sama tidak dianggap buku baru misal beda pencahayaan atau beda tata letak foto nya jadi pas scan buku yang sama tidak dianggap buku baru. paham nga ini fitur yang gua buat siang tadi

**Keterbatasan yang diketahui:**

1. **Baris lama dari notebook tidak bisa langsung dipromosikan**, karena `judul_asli` kosong. Jalurnya: `cache add` ulang judul dan penulis aslinya (status tidak tersentuh), lalu `cache promosi`.
2. **Notebook memakai `INSERT OR REPLACE` ke `cache_metadata`.** Baris `dipromosikan` kini dilindungi trigger, tapi baris lain yang ditimpa tetap kehilangan `judul_asli`, `penulis_asli`, dan `status`. Hal serupa berlaku untuk `cache_scan` begitu kolomnya bertambah.
3. **`rag.exe` di luar terminal MSYS2** butuh DLL dari `clang64\bin` di PATH.
4. **Proyek ada di folder OneDrive.** Sinkronisasi bisa mengunci atau menduplikasi file `.db` dan `.bak-*`, dan salinan data ikut ke cloud.
5. **Belum ada WAL.** `busy_timeout` hanya memberi napas, bukan antrean. Menurut dokumentasi SQLite, transaksi `BEGIN` biasa yang mulai dengan membaca lalu naik menulis (`cache promosi`, `cache ttl pasang`) bisa langsung gagal `SQLITE_BUSY` kalau ada penulis lain yang commit di antaranya. Pencegahnya `BEGIN IMMEDIATE`, belum dipakai dan belum diuji.
6. **`cache scan` belum punya masa berlaku.** Tabel `cache_scan` belum punya kolom waktu, jadi belum ada pembersihan berdasarkan umur yang bermakna.
7. **Backup dibuat sebelum validasi dan konfirmasi** untuk semua perintah tulis, jadi file `.bak-*` tetap terbentuk walau `hapus` dibatalkan atau hash tidak ditemukan. Rotasi membatasi jumlahnya.
8. **`scan add` dengan argumen kosong menimpa kolom lama menjadi `NULL`** (sama dengan `INSERT OR REPLACE` di notebook). Berbeda dari `rag_manual`, yang memakai `COALESCE`.
9. **Huruf besar-kecil kategori tidak dinormalisasi di C++.** Baris dari notebook memakai huruf kecil (misalnya `fiksi`), dan nilai yang diketik lewat CLI disimpan apa adanya.
10. **`cache ttl` tanpa aksi (tampil status) bukan perintah baca-saja.** Ia ikut menjalankan `pastikan_skema_metadata`, jadi bisa mengubah skema (migrasi kolom, memasang trigger pelindung) tanpa backup, padahal isinya hanya `SELECT`.
11. **Fuzzy C++: huruf kecil hanya ASCII dan Latin-1.** Dari 1.147 karakter (U+0080 ke atas, BMP) yang punya padanan huruf kecil di Python, hanya 30 yang ditangani C++. Sisanya tidak, termasuk Latin Extended A/B (Ł, Š, Ā), Sirilik, Latin Extended Additional (Vietnam), dan Yunani, serta `İ` yang di Python menjadi dua karakter. Dampaknya hanya muncul kalau huruf yang sama berbeda kapitalisasi antara hasil baca sampul dan data tersimpan, misalnya `Łódź Ścibor` vs `łódź ścibor` (Python 1,000, C++ 0,818). Untuk judul Indonesia dan Inggris praktis tidak berpengaruh.
12. **Fuzzy mengambil kandidat terbaik asal skornya lolos ambang**, tanpa memeriksa apakah ada dua buku yang sama-sama mirip. Risiko salah edisi sama dengan versi notebook.
13. **Jalur ISBN lokal memakai cache hanya kalau barisnya juga ada di `katalog`**, sama dengan notebook. Kalau buku itu belum ada di `katalog`, koreksi lewat promosi tidak terbaca di jalur ini, dan di notebook Open Library dicek sebelum `rag_manual`.
14. **Trigger (pelindung dan TTL) menempel di file DB**, bukan di program. Hanya aktif pada file yang sudah dijalankan `migrasi`, perintah tulis cache, atau `cache ttl pasang`-nya.
15. **Dua database dan dua alat tulis `rag_manual`:** `book_cache.db` (notebook dan C++) dan `data/book_catalog.db` (FastAPI), serta `rag_cli.py` dan CRUD C++ dengan aturan berbeda (lihat bagian FastAPI Lokal).

**Isu terbuka:**

- Satu sumber kebenaran untuk file DB antara notebook dan `rag.exe`. Saat ini notebook membuka salinannya sendiri (di Drive) dan `rag.exe` membuka salinan lokal, jadi isi keduanya bisa berbeda.
- Cara Python memanggil C++: subprocess (perlu `-y` untuk perintah yang bertanya, `stdin` dikosongkan, UTF-8 untuk keluaran, DLL clang64 di PATH), `ctypes`, atau service HTTP lokal. Belum diputuskan.
- Apakah backup sebaiknya dibuat setelah validasi, bukan sebelum.
- Apakah WAL dipakai, dan apakah DB harian dipindah keluar dari OneDrive dulu.
- Dua katalog (`books` dan `katalog`) dan dua alat tulis `rag_manual`.

**Rencana berikutnya:**

1. Pasang proteksi API key di `main.py` (`dependencies=[Depends(verify_local_api_key)]` pada `include_router`) dan perbaiki route `GET /katalog` supaya benar-benar terdaftar. Rapikan impor ganda.
2. Jembatan Python ke C++, tahap baca dulu (pencarian dan list), baru tulis.
3. Ganti `INSERT OR REPLACE` di notebook dengan upsert yang mempertahankan kolom baru.
4. Perbaiki bug `_normalisasi_kategori` di notebook (dari membaca kode: `non_fiksi` terbaca sebagai `fiksi`, belum diuji).
5. Masa berlaku untuk `cache_scan` (usulan, belum diputuskan): kolom `waktu_masuk` lewat migrasi cek-dulu-baru-`ALTER`, lalu `cache scan bersihkan` berbasis umur. Diuji di salinan DB dulu karena menyentuh tabel berisi data asli.
6. `BEGIN IMMEDIATE` untuk `cache promosi` dan `cache ttl pasang`. Masukkan `cache ttl` (tanpa aksi) ke daftar baca-saja.
7. Perluas normalisasi huruf kecil di `kemiripan.cpp` (tabel pemetaan huruf besar-kecil), atau huruf kecilkan di sisi pemanggil sebelum dikirim ke C++.
8. `struct Buku` di `model/`, lalu `daftar()` mengembalikan `std::vector<Buku>` alih-alih langsung mencetak.
9. Test tertulis untuk parser CSV, upsert, promosi, `cache scan`, dan pencarian (termasuk kasus fuzzy di atas).
10. Pindahkan DB harian ke luar OneDrive.
11. gua mau buat fitur baru jadi penghubung data anatara katalog dan ragmanual jadi semisal admin scan buku buku baru di rag manual ga ada buku tersebut terus model nyari tuh di open library ternyata ketemu dong jadi data buku tersebut di simpan di katalog dulu katalog tuh cache versi pencarian open library nah semisal katalog nyimpen data baru habis tuh si ragmanual itu bakal ambil data baru dari open library jadi kalo ada data baru disimpan dicache katalog habis tuh disimpan ke rag manual data katalog itu tidak permanen yang permanen cuma rag manual paham nga. ini disibut relasi antar kode atau relasi antar table

## Rencana Jangka Menengah/Panjang

Belum diimplementasikan, masih berupa daftar terbuka:

1. Perluasan RAG manual — memprioritaskan buku pelajaran SD/SMP/SMA dan buku keterampilan (DIY/prakarya), yang diperkirakan volumenya tinggi di perpustakaan sekolah namun jarang terindeks di Open Library.
2. Deteksi lokasi barcode yang lebih aktif (mis. sliding window atau localisation yang lebih toleran terhadap foto miring), menggantikan heuristik crop rasio-tetap saat ini.
3. Fallback OCR untuk teks "ISBN ..." sebagai jaring pengaman terakhir sebelum menyerah ke input manual.
4. Endpoint konfirmasi/tambah-ke-rag_manual langsung dari hasil `isbn_tidak_ketemu`, supaya data rag_manual tumbuh dari pemakaian nyata, bukan hanya input manual di notebook.
5. Penyeragaman perilaku stok di semua jalur input (scan, ISBN manual, barcode).
6. Semantic search berbasis embedding untuk pencarian katalog berbasis makna/tema — membutuhkan data tambahan berupa sinopsis singkat per buku (bukan isi buku penuh, untuk menghindari isu hak cipta). Rekomendasi TF-IDF yang ada sekarang sudah memperlihatkan keterbatasannya (lihat contoh "1984" di Isu terbuka sisi Python). Catatan: semantic search berguna untuk orang yang mencari buku lewat topik, sedangkan mencocokkan judul hasil baca sampul tetap pekerjaan fuzzy (ejaan), dan keduanya sebaiknya tidak dicampur.
7. Test case tertulis untuk mendeteksi regresi saat kode berubah, termasuk evaluasi kuantitatif fitur barcode dengan sampel lebih besar.
8. Rate limit & autentikasi yang lebih ketat di Live API.
9. Evaluasi arsitektur deployment produksi (Colab + ngrok belum cocok untuk operasional 24/7; opsi GPU cloud perlu estimasi biaya dan model penjadwalan sebelum digunakan produksi).

## Keamanan dan Kredensial

- Kunci dan token (`COLAB_API_KEY`, `LOCAL_API_KEY`, token ngrok) hanya di `.env` (lokal) dan Colab Secrets. Jangan ditulis di sel notebook, README, atau kode.
- Repo ini publik, jadi riwayat git ikut terbaca. Kunci yang pernah ter-commit harus di-rotate, bukan hanya dihapus dari file.
- `.gitignore` sudah mengabaikan `.env`, `*.db`, `*.bak-*`, `*.exe`, dan `rag/build/`. Aturan itu tidak mencabut file yang sudah ter-track sebelumnya (cek dengan `git ls-files`).
- Pada arsitektur target, React tidak boleh memegang rahasia apa pun. Hanya Django yang memanggil FastAPI dan engine data.
- Begitu data pengguna (bukan hanya data buku) masuk ke SQLite: file `.db` dan semua `.bak-*` berisi data itu, SQLite tidak terenkripsi secara bawaan, dan OneDrive menyalinnya ke cloud. TTL dan trigger cache dirancang untuk cache, jangan dipasang ke tabel data pengguna.

## Dependensi

Notebook:
```
pip install zxing-cpp
```

FastAPI lokal:
```
pip install -r requirements.txt
```
(`fastapi`, `uvicorn[standard]`, `python-multipart`, `requests`, `python-dotenv`, `scikit-learn`)

Engine C++: clang (MSYS2 CLANG64), SQLite 3.24 atau lebih baru (`mingw-w64-clang-x86_64-sqlite3`).

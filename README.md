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
OpenCV (`cv2.barcode`) sudah tersedia bawaan Colab (diverifikasi di OpenCV 5.0.0), tidak perlu instalasi tambahan.

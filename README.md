# Book Scanner — Progress & Roadmap

## Progress Sesi Sebelumnya

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
- Urutan pengecekan di `lengkapi_metadata`: cache → RAG manual → Open Library → null. RAG manual menjadi override, bukan sekadar fallback, karena kasus "salah edisi" (Open Library mencocokkan ke edisi terjemahan/berbeda) lebih sering terjadi daripada kasus "tidak ketemu sama sekali".
- Kolom ISBN ditambahkan ke `rag_manual`; fungsi tambah/cari data diperbarui untuk mendukungnya.

**Bug besar yang ditemukan & diperbaiki:**
- `search.json` Open Library mengembalikan field `publisher` sebagai gabungan seluruh penerbit dari seluruh edisi (bisa 100+ nama campur bahasa), bukan penerbit edisi spesifik — diperbaiki dengan `editions.json` + filter bahasa.
- Google Books API dihapus dari pipeline (kena limit kuota harian publik).
- Beberapa kali ditemukan sisa kode versi lama yang tidak terhapus penuh saat revisi fungsi (menyebabkan hasil RAG manual tertimpa kembali oleh Open Library) — sudah diperbaiki.

## Progress Hari Ini

- Basis data RAG manual diperluas dari 10 menjadi 175 judul, kemudian dipangkas menjadi 100 judul setelah menghapus entri yang datanya tidak lengkap.
- Sisa `null` pada beberapa entri (ISBN kosong) dikonfirmasi wajar — umum terjadi pada buku dari penerbit/penulis skala kecil yang sering tidak mendaftarkan ISBN resmi.
- Verifikasi silang salah satu entri ("A untuk Amanda", Annisa Ihsani) menemukan bahwa ISBN yang tercatat valid, tetapi merujuk pada cetakan ulang 2021, berbeda dari cetakan pertama 2016 yang lebih umum muncul di pencarian umum — penerbit tetap sama, hanya edisi cetak yang berbeda. Mengonfirmasi pentingnya verifikasi dari fisik buku, bukan sumber online umum.
- Struktur tabel `rag_manual` (kolom ISBN) dan fungsi terkait (`tambah_ke_rag_manual`, `cari_di_rag_manual`) disesuaikan agar mendukung pencarian berbasis ISBN, bukan hanya judul-penulis-penerbit.

## Isu Terbuka (lanjut sesi berikutnya)

- `NameError: cari_dari_isbn` muncul saat sel terkait (bagian jalur ISBN) belum dijalankan ulang setelah restart runtime — perlu dipastikan urutan eksekusi sel dari awal notebook setiap sesi baru.
- Pengisian ISBN untuk sisa entri RAG manual yang masih kosong belum selesai.

## Rencana Jangka Menengah/Panjang

Belum diimplementasikan, masih berupa daftar terbuka:

1. Perluasan RAG manual — memprioritaskan buku pelajaran SD/SMP/SMA dan buku keterampilan (DIY/prakarya), yang diperkirakan memiliki volume tinggi di perpustakaan sekolah namun jarang terindeks di Open Library.
2. Baca ISBN otomatis dari foto cover belakang.
3. Endpoint konfirmasi/koreksi di Live API.
4. Semantic search berbasis embedding untuk pencarian katalog berbasis makna/tema — membutuhkan data tambahan berupa sinopsis per buku, bukan hanya judul-penulis-penerbit. Data sinopsis untuk fitur ini perlu dibatasi pada ringkasan singkat (bukan isi buku penuh) untuk menghindari isu hak cipta.
5. Test case tertulis untuk mendeteksi regresi saat kode berubah.
6. Rate limit & autentikasi yang lebih ketat di Live API.
7. Evaluasi arsitektur deployment produksi (Colab + ngrok belum cocok untuk operasional 24/7; opsi GPU cloud perlu estimasi biaya dan model penjadwalan sebelum digunakan produksi).

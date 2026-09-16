# Book Scanner — Progress & Roadmap

## Progress Sesi Sebelumnya

**Arsitektur inti:**
- Scan cover (Qwen2-VL) → judul, penulis, kategori. Prompt versi longgar (instruksi positif, boleh jawab null) terbukti lebih akurat dari versi ketat.
- Penerbit tidak pernah dari tebakan Qwen — selalu dari Open Library (`editions.json`, difilter bahasa eng/ind) atau lookup ISBN langsung. Kalau tidak ketemu, `null` (dianggap benar, bukan gagal).
- Kategori dinormalisasi otomatis ke daftar baku (`self_help`, `fiksi`, dst), toleran ke variasi format/typo ringan.
- Cache 2 arah: `cache_scan` (by hash gambar) dan `cache_metadata` (by judul+penulis). ISBN dari input manual otomatis menular ke hasil scan cover berikutnya.
- Jalur ISBN langsung (`cari_dari_isbn`) dengan cache tersendiri.
- Katalog SQLite: hasil scan langsung tersimpan otomatis, dengan dedup — buku yang sama menambah stok, bukan menumpuk baris baru.
- Normalisasi pencocokan buku tahan aksen (é, dll) dan variasi nama penerbit (`Corgi` = `Corgi Books`).
- Live API (FastAPI + ngrok) stabil: fix asyncio Python 3.13, fix koneksi SQLite closed, resize+convert JPG di endpoint `/scan` untuk mencegah OOM dari foto resolusi tinggi.
- Migrasi skema tabel terpusat lewat `DAFTAR_MIGRASI`.

**Bug besar yang ditemukan & diperbaiki:**
- `search.json` Open Library mengembalikan field `publisher` sebagai gabungan seluruh penerbit dari seluruh edisi di berbagai negara (bisa 100+ nama campur bahasa), bukan penerbit edisi spesifik. Diperbaiki dengan `editions.json` + filter bahasa.
- Google Books API dihapus dari pipeline (kena limit kuota harian publik).
- Timeout Open Library dinaikkan dari 8 ke 15 detik.

## Progress Hari Ini

**Fitur RAG manual ditambahkan sebagai lapisan verifikasi:**
- Tabel `rag_manual` (SQLite) menyimpan pasangan judul-penulis-penerbit yang sudah diverifikasi manual.
- Pencarian pakai fuzzy string matching (`difflib`, bawaan Python) — toleran terhadap variasi kecil penulisan judul/penulis, tanpa perlu embedding model atau vector database.
- Urutan pengecekan di `lengkapi_metadata`: cache → **RAG manual → Open Library** → null. RAG manual jadi override, bukan sekadar fallback, karena kasus "salah edisi" (Open Library mencocokkan ke edisi terjemahan/berbeda) terjadi lebih sering daripada kasus "tidak ketemu sama sekali".
- 10 judul awal dimasukkan ke RAG manual (kombinasi fiksi Indonesia populer), sebagian sudah terverifikasi lewat pengecekan silang sebelumnya (Pulang, Cantik Itu Luka, Bumi Manusia), sebagian masih berdasarkan pengetahuan umum dan berstatus draf.
- Query RAG manual jauh lebih cepat dibanding Open Library karena murni pencarian database lokal tanpa network call.
- Ditemukan dan diperbaiki bug penggabungan kode yang menyebabkan hasil RAG manual tertimpa kembali oleh Open Library akibat sisa kode versi lama yang tidak terhapus.

**Verifikasi kasus "salah edisi" via RAG manual:**
- Cantik Itu Luka (Eka Kurniawan) — Open Library mengarah ke edisi terjemahan Inggris "Beauty is a Wound" (Text Publishing Company); RAG manual mengoreksi ke Gramedia Pustaka Utama.
- Laskar Pelangi, Filosofi Teras — terverifikasi cocok antara tebakan Qwen dan data RAG manual (beda format penulisan nama penerbit, substansi sama).

## Rencana Jangka Menengah/Panjang

Belum diimplementasikan, masih berupa daftar terbuka:

1. **Perluasan RAG manual** — menambah entri untuk judul-judul yang diketahui sering gagal/salah edisi di Open Library, terutama judul lokal/Indonesia.
2. **Baca ISBN otomatis dari foto cover belakang** — mengurangi ketergantungan pada input manual ISBN.
3. **Endpoint konfirmasi/koreksi di Live API** — saat ini koreksi katalog hanya bisa dilakukan dari dalam notebook.
4. **Semantic search berbasis embedding** — untuk pencarian katalog berbasis makna/tema, bukan kecocokan string; bisa dipakai ulang untuk fitur rekomendasi.
5. **Test case tertulis** — daftar skenario yang harus selalu lolos (cache hit, normalisasi kategori, dedup penerbit beda format, prioritas RAG manual atas Open Library) untuk mendeteksi regresi saat kode berubah.
6. **Rate limit & autentikasi yang lebih ketat di Live API** — relevan jika sistem diakses oleh lebih dari satu pihak.
7. **Evaluasi arsitektur deployment produksi** — Colab + ngrok cocok untuk pengembangan, belum untuk operasional 24/7; opsi GPU cloud (mis. GCP T4) perlu dipertimbangkan bersama estimasi biaya dan model penjadwalan (scheduled start/stop, spot instance) sebelum digunakan produksi.

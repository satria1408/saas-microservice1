# Book Scanner — Progress & Roadmap

## Progress Hari Ini

**Arsitektur inti:**
- Scan cover (Qwen2-VL) → judul, penulis, kategori. Prompt versi longgar (instruksi positif, boleh jawab null) terbukti lebih akurat dari versi ketat.
- Penerbit **tidak pernah** dari tebakan Qwen — selalu dari Open Library (`editions.json`, difilter bahasa eng/ind) atau lookup ISBN langsung. Kalau tidak ketemu, `null` (dianggap benar, bukan gagal).
- Kategori dinormalisasi otomatis ke daftar baku (`self_help`, `fiksi`, dst), toleran ke variasi format/typo ringan.
- Cache 2 arah: `cache_scan` (by hash gambar, skip Qwen kalau foto sama persis) dan `cache_metadata` (by judul+penulis, skip Open Library kalau sudah pernah dicari). ISBN dari input manual otomatis "menular" ke hasil scan cover berikutnya.
- Jalur ISBN langsung (`cari_dari_isbn`) — lebih presisi dari pencarian judul+penulis karena ISBN unik per edisi. Sudah ada cache-nya sendiri.
- Katalog SQLite: hasil scan langsung tersimpan otomatis (tidak menunggu konfirmasi), dengan dedup — buku yang sama menambah stok, bukan menumpuk baris baru.
- Normalisasi pencocokan buku tahan aksen (é, dll) dan variasi nama penerbit (`Corgi` = `Corgi Books`).
- Live API (FastAPI + ngrok) stabil: fix asyncio Python 3.13, fix koneksi SQLite closed, resize+convert JPG di endpoint `/scan` (cegah OOM dari foto resolusi tinggi HP).
- Migrasi skema tabel dirapikan jadi 1 daftar terpusat (`DAFTAR_MIGRASI`), tidak perlu tulis ulang blok migrasi tiap nambah kolom.

**Bug besar yang ditemukan & diperbaiki:**
- `search.json` Open Library mengembalikan field `publisher` sebagai gabungan SEMUA penerbit dari SEMUA edisi di seluruh dunia (bisa 100+ nama campur bahasa) — bukan penerbit edisi spesifik. Diperbaiki dengan `editions.json` + filter bahasa.
- Google Books API dihapus dari pipeline (kena limit kuota harian publik).
- Beberapa kasus "salah edisi" (Cantik Itu Luka, El Principito, Fahrenheit 451) — Open Library kadang mencocokkan ke edisi terjemahan/cetakan berbeda dari fisik buku yang di-scan. Solusi jangka pendek: verifikasi manual + ISBN langsung dari fisik buku.

## Rencana Jangka Menengah/Panjang (usulan awal, masih perlu didiskusikan)

Ini draf ide dariku sebagai bahan mentah — silakan direvisi total sebelum dituang ke diagram resmi:

1. **RAG basis manual** — fallback terakhir untuk buku yang gagal total di Open Library (contoh nyata: "Pulang" sebelum ketemu lewat ISBN). Sumber data: histori koreksi petugas + input manual buku yang sering hilang di database publik (kebanyakan judul Indonesia/lokal).
2. **Baca ISBN otomatis dari foto cover belakang** — supaya petugas tidak perlu ketik manual; bisa pakai OCR ringan khusus baca deret angka (lebih sederhana dari OCR baca nama penerbit yang sudah terbukti tidak reliable).
3. **Endpoint konfirmasi/koreksi di Live API** — sekarang koreksi (`update_status_konfirmasi`) baru bisa dipanggil dari dalam notebook; perlu endpoint tersendiri kalau mau dikontrol dari FastAPI hosting/UI admin.
4. **Semantic search sederhana** — fondasi untuk pencarian katalog ("cari buku tentang pertumbuhan diri"), bisa dipakai ulang untuk fitur rekomendasi.
5. **Test case tertulis** — daftar kasus yang harus selalu lolos (skenario cache hit, normalisasi kategori, dedup penerbit beda format) supaya perubahan kode ke depan tidak sengaja merusak yang sudah benar.
6. **Rate limit & auth yang lebih ketat di Live API** — mengingat sistem ini akan diakses banyak pihak (petugas + kemungkinan integrasi lain).

Detail arsitektur & keputusan menyusul di diagram terpisah.

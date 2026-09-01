# Book Scanner API

Sistem scan sampul buku otomatis pakai vision AI (Qwen2-VL), dilengkapi
fitur penjelasan isi buku dan rekomendasi. Dibangun bertahap, didokumentasikan
di sini biar gampang dilanjutin kapan aja.

## Status saat ini tanggal 01/09-2026

| Fitur | Status | Akurasi/Catatan |
|---|---|---|
| Scan sampul (judul/penulis/penerbit) | ✅ Matang | 93% akurat (tested di 15 sample) |
| Jelaskan isi buku (sinopsis + parafrase AI) | ✅ Jalan | Sinopsis dari Google Books/Open Library |
| Katalog buku (database lokal) | ✅ Jalan | SQLite, baru mulai diisi |
| Rekomendasi buku (TF-IDF similarity) | ⚠️ Dibuat, **BELUM DITES** | Ada bug potensial, lihat "Known Issues" |

## Arsitektur

Ada 2 bagian yang jalan terpisah:

1. **Google Colab** (otak AI) - jalanin model Qwen2-VL, di-expose ke internet
   lewat ngrok tunnel dengan domain tetap. Proteksi pakai API key di header
   `x-api-key`.
2. **FastAPI lokal** (di laptop) - terima request dari user, panggil Colab
   buat proses AI, simpan hasil ke katalog SQLite lokal, dan sediakan
   endpoint rekomendasi.

```
User -> FastAPI lokal (/scan-buku) -> Colab (via ngrok, /scan) -> Qwen2-VL
                |
                v
         book_catalog.db (SQLite lokal)
                |
                v
       /rekomendasi/{judul} (TF-IDF)
```

## Struktur folder (FastAPI lokal)

```
book-scanner-api/
├── main.py                          # entry point, daftar semua router
├── config.py                        # baca .env
├── requirements.txt
├── .env                             # JANGAN commit ke git
├── services/
│   ├── colab_scanner_client.py      # panggil Colab via ngrok
│   ├── book_catalog.py              # simpan/baca katalog SQLite
│   └── recommendation.py            # TF-IDF similarity
├── routers/
│   ├── scan.py                      # POST /scan-buku
│   └── rekomendasi.py               # GET /rekomendasi/{judul}
└── data/
    └── book_catalog.db              # otomatis dibuat saat pertama jalan
```

## Notebook Colab

File: `scan_buku_qwen2vl_test.ipynb`

Urutan wajib tiap buka sesi baru:
1. Bagian 1-5: mount Drive, install dependencies, load model, prompt, `ask_model`
2. Bagian 11: fungsi cari sinopsis (Google Books + Open Library) + `explain_book`
3. Bagian 10 (paling akhir): nyalain server + tunnel ngrok - cell ini
   MENGUNCI notebook selama jalan, jangan di-stop selama masih dipakai

**Isi dulu sebelum jalanin bagian 10:**
- `NGROK_AUTHTOKEN` - dari dashboard ngrok.com
- `API_KEY` - bikin sendiri, string acak (contoh: `secrets.token_urlsafe(32)`)
- Domain ngrok yang dipakai: `closable-bullish-showman.ngrok-free.dev`

## Cara jalanin FastAPI lokal

```bash
pip install -r requirements.txt
uvicorn main:app --reload
```

Buka `http://127.0.0.1:8000/docs` buat tes endpoint.

**PENTING:** Colab harus dinyalain DULU (sampai muncul "Live API aktif")
sebelum FastAPI lokal bisa berhasil scan - kalau Colab mati, endpoint
`/scan-buku` bakal balikin error 503.

## Perjalanan sampai ke versi stabil (biar nggak diulang kesalahannya)

Prompt awal (tanpa parameter generate tambahan) sudah mencapai 93% akurat.
Sempat dicoba tuning `repetition_penalty`/`no_repeat_ngram_size` untuk
menangani 1 kasus gagal (looping di 1 buku), tapi ini malah **menurunkan**
akurasi keseluruhan karena mengubah perilaku SEMUA generate, bukan cuma
kasus yang bermasalah.

**Pelajaran:** kalau nemu 1 kasus gagal, jangan langsung ubah parameter
generate global. Tambahkan retry+validasi sebagai jaring pengaman (tidak
mengubah generate() itu sendiri), dan selalu re-test ke SEMUA sample,
bukan cuma kasus yang tadinya gagal.

Versi final yang dipakai sekarang:
- `model.generate()` polos, TANPA `repetition_penalty`/`no_repeat_ngram_size`
- Retry otomatis (maks 2x) + `json_repair` + normalisasi nama kunci sebagai
  jaring pengaman kalau sesekali ada output yang gagal di-parse
- Field `kategori` di-drop dari prompt scan (disederhanakan, fokus 3 field inti)

## Known Issues / TODO

1. **[BELUM DITES] Bug potensial di `recommendation.py`**: `TfidfVectorizer`
   bisa error `ValueError: empty vocabulary` kalau SEMUA buku di katalog
   punya `penjelasan` kosong/null. Perlu ditambah guard sebelum dipakai
   serius - cek dulu apakah ada teks yang valid sebelum fit vectorizer.
2. Endpoint `/scan` di Colab publik (siapa saja yang punya URL ngrok bisa
   akses) - sudah dikasih proteksi API key, tapi tetap perhatikan traffic
   asing di log kalau tunnel lama nyala.
3. Domain ngrok gratis butuh sesi Colab tetap terbuka - kalau mau
   production sungguhan (bukan sekadar testing), perlu pindah ke infra
   yang lebih permanen (dibahas: Oracle Cloud Always Free, HF Spaces -
   saat ini masih terkunci perlu PRO, atau serverless GPU berbayar).
4. Fitur "rangkum buku" kadang menghasilkan penjelasan yang mendaftar
   semua judul bab/cerita alih-alih merangkum tema secara umum - sudah
   ditambah instruksi + validasi panjang kalimat, tapi belum divalidasi
   di banyak sample.
5. Kategori genre (untuk rekomendasi yang lebih baik) belum ada di hasil
   scan - saat ini rekomendasi murni dari kemiripan teks `penjelasan`.

## Rencana lanjutan

- Uji `/rekomendasi/{judul}` dengan katalog berisi minimal 5-10 buku variatif
- Perbaiki guard `recommendation.py` untuk kasus katalog kosong/minim
- Pertimbangkan tambah field `kategori` lagi ke scan (dengan disiplin:
  ubah satu hal, test ke semua sample, baru lanjut)
- Pisahkan fitur jadi service lebih modular kalau sudah stabil semua
  (rencana yang sempat disebut sebelumnya)
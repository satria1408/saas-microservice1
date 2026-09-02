# Book Scanner API

Sistem scan sampul buku otomatis pakai vision AI (Qwen2-VL), dilengkapi
fitur penjelasan isi buku, katalog, dan rekomendasi. Dibangun bertahap,
didokumentasikan di sini biar gampang dilanjutin kapan aja.

## Status saat ini

| Fitur | Status | Akurasi/Catatan |
|---|---|---|
| Scan sampul (judul/penulis/penerbit) | ✅ Matang | 93% akurat (tested di 15 sample) |
| Jelaskan isi buku (sinopsis + parafrase AI) | ✅ Jalan | Sinopsis dari Google Books/Open Library. Sering gagal untuk buku lokal kecil - ini keterbatasan data, bukan bug |
| Katalog buku (database lokal) | ✅ Jalan | SQLite, data lama yang cacat sudah dibersihkan |
| Rekomendasi buku (TF-IDF similarity) | ✅ **Sudah dites, jalan** | Lihat catatan akurasi tematik di "Known Issues" |
| Keamanan endpoint | ✅ Jalan | Endpoint Colab DAN endpoint lokal sekarang sama-sama pakai API key terpisah |

## Arsitektur

Ada 2 bagian yang jalan terpisah:

1. **Google Colab** (otak AI) - jalanin model Qwen2-VL, di-expose ke internet
   lewat ngrok tunnel dengan domain tetap. Proteksi pakai API key
   (`COLAB_API_KEY`) di header `x-api-key`.
2. **FastAPI lokal** (di laptop) - terima request dari user, panggil Colab
   buat proses AI, simpan hasil ke katalog SQLite lokal, dan sediakan
   endpoint rekomendasi. Proteksi pakai API key TERPISAH (`LOCAL_API_KEY`).

```
User -> FastAPI lokal (/scan-buku, butuh x-api-key LOCAL_API_KEY)
                |
                v
        Colab (via ngrok, /scan, butuh x-api-key COLAB_API_KEY)
                |
                v
             Qwen2-VL
                |
                v
      book_catalog.db (SQLite lokal, di laptop)
                |
                v
       /rekomendasi/{judul} (TF-IDF, JALAN TANPA COLAB)
```

**Penting:** katalog dan rekomendasi TIDAK butuh Colab menyala - keduanya
murni beroperasi di database lokal. Yang benar-benar butuh Colab hidup
hanya momen scan gambar baru.

## Struktur folder (FastAPI lokal)

```
book-scanner-api/
├── main.py                          # entry point, daftar semua router + proteksi
├── config.py                        # baca .env
├── requirements.txt
├── .env                             # JANGAN commit ke git
├── .gitignore
├── services/
│   ├── auth.py                      # verifikasi LOCAL_API_KEY
│   ├── colab_scanner_client.py      # panggil Colab via ngrok
│   ├── book_catalog.py              # simpan/baca katalog SQLite
│   └── recommendation.py            # TF-IDF similarity
├── routers/
│   ├── scan.py                      # POST /scan-buku
│   └── rekomendasi.py               # GET /rekomendasi/{judul}, GET /katalog
├── scripts/
│   └── bersihkan_placeholder.py     # one-off cleanup data cacat lama
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
- `API_KEY` (jadi `COLAB_API_KEY` di sisi FastAPI lokal) - string acak
  (contoh: `secrets.token_urlsafe(32)`)
- Domain ngrok yang dipakai: `closable-bullish-showman.ngrok-free.dev`

**Kalau kredensial pernah ke-expose (misal ter-paste ke tempat yang tidak
seharusnya), SELALU rotate/regenerate - jangan dipakai lagi.**

## Cara jalanin FastAPI lokal

```bash
pip install -r requirements.txt
uvicorn main:app --reload
```

Buka `http://127.0.0.1:8000/docs` buat tes endpoint. Semua endpoint sekarang
butuh header `x-api-key` diisi `LOCAL_API_KEY` dari `.env`.

**PENTING:** Colab harus dinyalain DULU (sampai muncul "Live API aktif")
sebelum FastAPI lokal bisa berhasil scan - kalau Colab mati, endpoint
`/scan-buku` bakal balikin error 503.

## Perjalanan sampai ke versi stabil (biar nggak diulang kesalahannya)

Prompt awal (tanpa parameter generate tambahan) sudah mencapai 93% akurat.
Sempat dicoba tuning `repetition_penalty`/`no_repeat_ngram_size` untuk
menangani 1 kasus gagal (looping di 1 buku), tapi ini malah **menurunkan**
akurasi keseluruhan karena mengubah perilaku SEMUA generate, bukan cuma
kasus yang bermasalah.

**Pelajaran #1:** kalau nemu 1 kasus gagal, jangan langsung ubah parameter
generate global. Tambahkan retry+validasi sebagai jaring pengaman (tidak
mengubah generate() itu sendiri), dan selalu re-test ke SEMUA sample,
bukan cuma kasus yang tadinya gagal.

**Pelajaran #2:** cache itu HANYA boleh menyimpan hasil yang BENAR-BENAR
berhasil. Sempat ada bug di 2 tempat terpisah (`book_catalog.db` lokal
dan `book_explain_cache.db` di Colab) di mana kalimat placeholder
"sinopsis tidak ditemukan" ikut ke-cache/ke-catalog sebagai kalau itu
data valid - akibatnya kegagalan jadi permanen (tidak pernah dicoba ulang)
dan fitur rekomendasi bisa keliru menganggap 2 buku yang sama-sama gagal
cari sinopsis sebagai "mirip kontennya". Sudah diperbaiki di kedua tempat
dan data lama sudah dibersihkan.

Versi final yang dipakai sekarang:
- `model.generate()` polos, TANPA `repetition_penalty`/`no_repeat_ngram_size`
- Retry otomatis (maks 2x) + `json_repair` + normalisasi nama kunci sebagai
  jaring pengaman kalau sesekali ada output yang gagal di-parse
- Field `kategori` di-drop dari prompt scan (disederhanakan, fokus 3 field inti)
- Cache/katalog hanya diisi kalau `sumber_sinopsis` benar-benar ada isinya

## Known Issues / TODO

1. **Akurasi tematik rekomendasi masih kasar.** TF-IDF murni cuma
   mencocokkan kata yang muncul, bukan makna/genre. Contoh nyata: "1984"
   (fiksi distopia) pernah dapat skor kemiripan tertinggi dengan
   "How to Win Friends & Influence People" (self-help) - dua genre yang
   jauh berbeda. Bukan bug, tapi keterbatasan pendekatan yang dipakai.
   Upgrade ke embedding semantik bisa dipertimbangkan nanti kalau worth
   effort-nya.
2. **Ketergantungan pada sesi Colab tetap ada** untuk momen scan gambar
   baru. Sudah dicoba beberapa alternatif gratis (Ollama lokal - laptop
   tidak kuat; Hugging Face Spaces - sekarang perlu PRO subscription
   untuk SDK Docker/Gradio; Oracle Cloud Always Free - butuh kartu untuk
   verifikasi, tidak tersedia). Perpusnas/OneSearch tidak punya API
   publik resmi untuk data buku, jadi tidak bisa jadi alternatif sumber
   data maupun compute. Opsi paling realistis yang belum diimplementasi:
   pindah dari mode "live API" ke mode "batch" (admin buka Colab
   sesekali, proses beberapa scan sekaligus, bukan harus nyala 24/7).
3. Endpoint `/scan` di Colab publik (siapa saja yang punya URL ngrok bisa
   akses) - sudah dikasih proteksi API key di kedua sisi (Colab dan
   lokal), tapi tetap perhatikan traffic asing di log kalau tunnel lama
   nyala.
4. Fitur "rangkum buku" kadang menghasilkan penjelasan yang mendaftar
   semua judul bab/cerita alih-alih merangkum tema secara umum - sudah
   ditambah instruksi + validasi panjang kalimat, tapi belum divalidasi
   di banyak sample.
5. Kategori genre (untuk rekomendasi yang lebih baik) belum ada di hasil
   scan - saat ini rekomendasi murni dari kemiripan teks `penjelasan`.

## Rencana lanjutan

- Pindah ke mode batch untuk mengurangi ketergantungan sesi Colab yang
  harus terus menyala
- Uji `/rekomendasi/{judul}` dengan katalog yang lebih besar (5-10+ buku
  variatif) untuk melihat pola akurasi tematik lebih jelas
- Pertimbangkan tambah field `kategori` lagi ke scan (dengan disiplin:
  ubah satu hal, test ke semua sample, baru lanjut)
- Pertimbangkan upgrade rekomendasi dari TF-IDF ke embedding semantik
  kalau akurasi tematik jadi prioritas
- Pisahkan fitur jadi service lebih modular kalau sudah stabil semua
  (rencana yang sempat disebut sebelumnya)
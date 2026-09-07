Riwayat Update

3 Sept — Fix fitur rekomendasi buku (TF-IDF) sampai jalan normal.

4-6 Sept — Nambahin fitur AI parafrase buat penjelasan isi buku. Ketemu bug: hasil parafrase kadang kepotong nyalin frasa mentah dari sinopsis asli (potensi copyright). Dibenerin pakai fungsi cek copyright (bagian 8) — sekarang sebelum dikasih ke user, AI "mikir dulu": dicek dulu ada frasa yang sama persis apa nggak sama sinopsis asli, kalau ketemu langsung retry sampai beneran diparafrase, bukan nyalin.
Kendala lain: hasil penjelasan sering ke-flag gagal (false) padahal isinya udah bagus, gara-gara teks kepotong duluan sebelum kalimatnya selesai. Solusinya: naikin jumlah token generate, sama naikin batas panjang validasi dari 400 ke 1000 karakter.
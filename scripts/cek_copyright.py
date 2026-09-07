"""
Script buat validasi manual: cek apakah hasil parafrase AI (penjelasan)
menyalin frasa panjang dari sinopsis asli (isu hak cipta).

Cara pakai:
    python scripts/cek_copyright.py "Judul Buku" "Nama Penulis" "path/ke/hasil_penjelasan.txt"

Atau edit langsung nilai JUDUL, PENULIS, PENJELASAN di bawah lalu jalankan:
    python scripts/cek_copyright.py
"""

import sys
import os

sys.path.append(os.path.join(os.path.dirname(__file__), ".."))

from services.copyright_check import get_synopsis_for_check, cek_potensi_copyright


def main():
    # Ganti 3 nilai ini sesuai buku yang mau dicek, atau pakai argumen command line
    JUDUL = "The Alchemist"
    PENULIS = "Paulo Coelho"
    PENJELASAN = "isi penjelasan hasil AI dari scan_and_explain, paste di sini"

    if len(sys.argv) >= 4:
        JUDUL, PENULIS, PENJELASAN = sys.argv[1], sys.argv[2], sys.argv[3]

    sinopsis_asli = get_synopsis_for_check(JUDUL, PENULIS)

    if not sinopsis_asli:
        print(f"Sinopsis untuk '{JUDUL}' tidak ditemukan - tidak bisa divalidasi.")
        return

    hasil = cek_potensi_copyright(sinopsis_asli, PENJELASAN)
    print(f"=== {JUDUL} ===")
    print(f"Aman: {hasil['aman']}")
    print(f"Jumlah frasa mencurigakan: {hasil['jumlah_frasa_mencurigakan']}")
    if hasil["contoh_frasa"]:
        print("Contoh frasa bermasalah:")
        for f in hasil["contoh_frasa"]:
            print(f"  - {f}")


if __name__ == "__main__":
    main()
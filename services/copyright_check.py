# services/copyright_check.py
import re
import requests


def get_synopsis_for_check(judul: str, penulis: str) -> str | None:
    """Versi ringan cari sinopsis - HANYA buat validasi copyright di lokal,
    bukan buat dipakai user-facing (itu tugas Colab)."""
    query = f"intitle:{judul}"
    if penulis:
        query += f"+inauthor:{penulis}"
    url = f"https://www.googleapis.com/books/v1/volumes?q={requests.utils.quote(query)}&maxResults=3"

    try:
        res = requests.get(url, timeout=8)
        if res.status_code == 200:
            for item in res.json().get("items", []):
                desc = item.get("volumeInfo", {}).get("description")
                if desc:
                    return desc
    except Exception:
        pass
    return None


def cek_potensi_copyright(sinopsis_mentah: str, penjelasan: str, min_kata_mencurigakan: int = 8) -> dict:
    kata_sinopsis = re.sub(r"[^\w\s]", "", sinopsis_mentah.lower()).split()
    kata_penjelasan = re.sub(r"[^\w\s]", "", penjelasan.lower()).split()

    frasa_sinopsis = {
        " ".join(kata_sinopsis[i:i + min_kata_mencurigakan])
        for i in range(len(kata_sinopsis) - min_kata_mencurigakan + 1)
    }

    frasa_bermasalah = [
        " ".join(kata_penjelasan[i:i + min_kata_mencurigakan])
        for i in range(len(kata_penjelasan) - min_kata_mencurigakan + 1)
        if " ".join(kata_penjelasan[i:i + min_kata_mencurigakan]) in frasa_sinopsis
    ]

    return {
        "aman": len(frasa_bermasalah) == 0,
        "jumlah_frasa_mencurigakan": len(frasa_bermasalah),
        "contoh_frasa": frasa_bermasalah[:3],
    }
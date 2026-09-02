from fastapi import APIRouter, HTTPException
from services.recommendation import get_recommendations
from services.book_catalog import get_all_books

router = APIRouter()


@router.get("/rekomendasi/{judul}")
def rekomendasi_buku(judul: str, top_n: int = 3):
    hasil = get_recommendations(judul, top_n=top_n)

    if not hasil:
        return {
            "judul_acuan": judul,
            "rekomendasi": [],
            "pesan": "Belum ada rekomendasi - buku tidak ditemukan di katalog, "
                     "atau katalog masih terlalu sedikit isinya.",
        }

    return {"judul_acuan": judul, "rekomendasi": hasil}

    @router.get("/katalog")
    def lihat_katalog():
        return get_all_books()
from fastapi import APIRouter, File, UploadFile, HTTPException
from services.colab_scanner_client import scan_book_cover, ColabScannerUnavailable
from services.book_catalog import add_book_to_catalog

router = APIRouter()


@router.post("/scan-buku")
async def scan_buku(file: UploadFile = File(...)):
    image_bytes = await file.read()

    try:
        hasil = scan_book_cover(image_bytes, filename=file.filename)
    except ColabScannerUnavailable as e:
        raise HTTPException(status_code=503, detail=str(e))

    if hasil.get("judul"):
        penjelasan_untuk_katalog = hasil.get("penjelasan") if hasil.get("sumber_sinopsis") else None

        add_book_to_catalog(
            judul=hasil["judul"],
            penulis=hasil.get("penulis"),
            penerbit=hasil.get("penerbit"),
            penjelasan=penjelasan_untuk_katalog,
        )

    return hasil
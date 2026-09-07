import requests
from config import COLAB_SCANNER_URL, COLAB_API_KEY

REQUEST_TIMEOUT_SECONDS = 60


class ColabScannerUnavailable(Exception):
    pass


def scan_book_cover(image_bytes: bytes, filename: str = "cover.jpg") -> dict:
    try:
        response = requests.post(
            COLAB_SCANNER_URL,
            files={"file": (filename, image_bytes, "image/jpeg")},
            headers={"x-api-key": COLAB_API_KEY},
            timeout=REQUEST_TIMEOUT_SECONDS,
        )

        if response.status_code >= 400:
            # Coba baca detail error dari body JSON Colab dulu, sebelum
            # raise_for_status() membuang informasi itu
            try:
                detail = response.json().get("error", response.text)
            except Exception:
                detail = response.text
            raise ColabScannerUnavailable(f"Colab error ({response.status_code}): {detail}")

        return response.json()

    except requests.exceptions.ConnectionError:
        raise ColabScannerUnavailable(
            "Tidak bisa terhubung ke Colab. Pastikan sesi Colab dan cell "
            "tunnel ngrok masih aktif."
        )
    except requests.exceptions.Timeout:
        raise ColabScannerUnavailable("Colab tidak merespons dalam waktu yang wajar.")
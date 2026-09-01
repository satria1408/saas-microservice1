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
        response.raise_for_status()
        return response.json()

    except requests.exceptions.ConnectionError:
        raise ColabScannerUnavailable(
            "Tidak bisa terhubung ke Colab. Pastikan sesi Colab dan cell "
            "tunnel ngrok masih aktif."
        )
    except requests.exceptions.Timeout:
        raise ColabScannerUnavailable("Colab tidak merespons dalam waktu yang wajar.")
    except requests.exceptions.HTTPError as e:
        raise ColabScannerUnavailable(f"Colab merespons dengan error: {e}")
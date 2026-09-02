import os
from dotenv import load_dotenv

load_dotenv()

COLAB_SCANNER_URL = os.environ.get("COLAB_SCANNER_URL", "")
COLAB_API_KEY = os.environ.get("COLAB_API_KEY", "")
LOCAL_API_KEY = os.environ.get("LOCAL_API_KEY", "") 

# Path database SQLite - cukup path file biasa, TIDAK butuh server/koneksi terpisah
DB_PATH = os.path.join(os.path.dirname(__file__), "data", "book_catalog.db")
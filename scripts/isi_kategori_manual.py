import sqlite3
import os

DB_PATH = os.path.join(os.path.dirname(__file__), "..", "data", "book_catalog.db")

# Isi manual berdasar hasil testing Colab sebelumnya
kategori_manual = {
    "1984": "fiksi",
    "how to win friends & influence people": "self_help",
    "seporsi mie ayam sebelum mati": "fiksi",
    "the adventures of sherlock holmes": "fiksi",
    # tambahin buku lain yang ada di katalog lu sesuai kebutuhan
}

conn = sqlite3.connect(DB_PATH)
for judul, kategori in kategori_manual.items():
    cursor = conn.execute(
        "UPDATE books SET kategori = ? WHERE judul_normalized = ?",
        (kategori, judul.replace(" ", "").replace("&", "").lower()),
    )
    print(f"{judul}: {cursor.rowcount} baris diupdate")

conn.commit()
conn.close()
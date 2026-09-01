import sqlite3
import os
import re
from config import DB_PATH


def init_catalog_db():
    """
    Ini yang disebut 'koneksi' di SQLite: buka file (dibuat otomatis kalau
    belum ada), bikin tabel kalau belum ada, tutup lagi. TIDAK ada server
    terpisah yang perlu dinyalakan - filenya sendiri YANG JADI database.
    """
    os.makedirs(os.path.dirname(DB_PATH), exist_ok=True)  # bikin folder data/ kalau belum ada
    conn = sqlite3.connect(DB_PATH)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS books (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            judul TEXT NOT NULL,
            judul_normalized TEXT UNIQUE,
            penulis TEXT,
            penerbit TEXT,
            kategori TEXT,
            penjelasan TEXT,
            ditambahkan_pada TEXT DEFAULT CURRENT_TIMESTAMP
        )
    """)
    conn.commit()
    conn.close()


def _normalize_title(judul: str) -> str:
    return re.sub(r"[^a-z0-9]", "", judul.lower())


def add_book_to_catalog(judul: str, penulis: str = None, penerbit: str = None,
                          kategori: str = None, penjelasan: str = None) -> dict:
    conn = sqlite3.connect(DB_PATH)
    conn.execute(
        """
        INSERT INTO books (judul, judul_normalized, penulis, penerbit, kategori, penjelasan)
        VALUES (?, ?, ?, ?, ?, ?)
        ON CONFLICT(judul_normalized) DO UPDATE SET
            penulis=excluded.penulis,
            penerbit=excluded.penerbit,
            kategori=excluded.kategori,
            penjelasan=excluded.penjelasan
        """,
        (judul, _normalize_title(judul), penulis, penerbit, kategori, penjelasan),
    )
    conn.commit()
    conn.close()
    return {"status": "tersimpan", "judul": judul}


def get_all_books() -> list[dict]:
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    rows = conn.execute("SELECT * FROM books ORDER BY ditambahkan_pada DESC").fetchall()
    conn.close()
    return [dict(row) for row in rows]
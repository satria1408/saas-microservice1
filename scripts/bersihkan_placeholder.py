
import sqlite3
import os

DB_PATH = os.path.join(os.path.dirname(__file__), "..", "data", "book_catalog.db")


def main():
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.execute(
        "UPDATE books SET penjelasan = NULL WHERE penjelasan LIKE 'Maaf, sinopsis%'"
    )
    conn.commit()
    print(f"Selesai - {cursor.rowcount} baris dibersihkan.")
    conn.close()


if __name__ == "__main__":
    main()
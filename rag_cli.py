#!/usr/bin/env python3
"""
rag_cli.py - kelola tabel rag_manual langsung dari terminal (tanpa Colab).

Logika insert/update sama dengan tambah_ke_rag_manual() di notebook:
buku dianggap sama kalau judul + penulis sama (tidak peduli huruf besar/kecil).
Kalau sudah ada -> di-update, kalau belum -> di-insert.

Lokasi database (urutan prioritas):
  1. opsi --db /path/book_cache.db
  2. environment variable RAG_DB
  3. ./book_cache.db (folder tempat kamu menjalankan script)

Contoh:
  python rag_cli.py add                                   # mode interaktif
  python rag_cli.py add "Filosofi Teras" "Henry Manampiring" "Kompas" 9786024125189
  python rag_cli.py import buku.csv                       # kolom: judul,penulis,penerbit,isbn
  python rag_cli.py import-calls hasil_ai.txt             # baris tambah_ke_rag_manual(...)
  python rag_cli.py list --cari laskar
  python rag_cli.py hapus 12
  python rag_cli.py cek
"""
import argparse
import ast
import csv
import os
import shutil
import sqlite3
import sys
from datetime import datetime


# ---------------------------------------------------------------- DB helpers
def cari_path_db(arg_db):
    return arg_db or os.environ.get("RAG_DB") or "book_cache.db"


def koneksi(path, backup=True):
    if not os.path.exists(path):
        print(f"[ERROR] File database tidak ditemukan: {path}")
        print("        Pakai --db /path/ke/book_cache.db atau set env RAG_DB.")
        sys.exit(1)
    if backup:
        stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        cadangan = f"{path}.bak-{stamp}"
        shutil.copy2(path, cadangan)
        print(f"[backup] {cadangan}")
    conn = sqlite3.connect(path)
    conn.execute(
        """CREATE TABLE IF NOT EXISTS rag_manual (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            judul TEXT, penulis TEXT, penerbit TEXT,
            sumber TEXT DEFAULT 'input_manual',
            waktu_masuk TEXT DEFAULT CURRENT_TIMESTAMP)"""
    )
    kolom = [r[1] for r in conn.execute("PRAGMA table_info(rag_manual)").fetchall()]
    if "isbn" not in kolom:
        conn.execute("ALTER TABLE rag_manual ADD COLUMN isbn TEXT")
    conn.commit()
    return conn


# ---------------------------------------------------------------- validasi
def bersihkan_isbn(isbn):
    if isbn is None:
        return None
    s = str(isbn).strip().replace("-", "").replace(" ", "")
    if s == "" or s.lower() in ("null", "none"):
        return None
    return s


def ean13_valid(s):
    if not s or len(s) != 13 or not s.isdigit():
        return False
    total = sum(int(d) * (1 if i % 2 == 0 else 3) for i, d in enumerate(s[:12]))
    return (10 - total % 10) % 10 == int(s[12])


def proses_isbn(isbn, paksa=False):
    """Balikin (isbn_final, pesan_peringatan). ISBN invalid -> None kecuali --paksa."""
    s = bersihkan_isbn(isbn)
    if s is None:
        return None, None
    if ean13_valid(s) or paksa:
        return s, None
    return None, f"ISBN '{s}' tidak lolos checksum EAN-13, dikosongkan"


# ---------------------------------------------------------------- operasi inti
def tambah(conn, judul, penulis, penerbit, isbn=None, sumber="input_manual", paksa=False):
    judul = (judul or "").strip()
    penulis = (penulis or "").strip()
    penerbit = (penerbit or "").strip() or None
    if not judul:
        print("[SKIP] judul kosong")
        return "skip"

    isbn_final, peringatan = proses_isbn(isbn, paksa)
    if peringatan:
        print(f"[WARN] {judul}: {peringatan}")

    ada = conn.execute(
        "SELECT id FROM rag_manual WHERE LOWER(judul)=LOWER(?) AND LOWER(penulis)=LOWER(?)",
        (judul, penulis),
    ).fetchone()
    if ada:
        conn.execute(
            "UPDATE rag_manual SET penerbit=?, isbn=?, sumber=? WHERE id=?",
            (penerbit, isbn_final, sumber, ada[0]),
        )
        print(f"[update] #{ada[0]} {judul} - {penerbit} - ISBN: {isbn_final}")
        return "update"
    cur = conn.execute(
        "INSERT INTO rag_manual (judul, penulis, penerbit, isbn, sumber) VALUES (?,?,?,?,?)",
        (judul, penulis, penerbit, isbn_final, sumber),
    )
    print(f"[baru]   #{cur.lastrowid} {judul} - {penerbit} - ISBN: {isbn_final}")
    return "baru"


def laporan(hitung):
    print(f"\nSelesai: {hitung['baru']} baru, {hitung['update']} diupdate, {hitung['skip']} dilewati.")


# ---------------------------------------------------------------- perintah
def cmd_add(a):
    conn = koneksi(cari_path_db(a.db), backup=not a.no_backup)
    if a.judul:
        tambah(conn, a.judul, a.penulis, a.penerbit, a.isbn, paksa=a.paksa)
        conn.commit()
        return
    print("Mode interaktif. Kosongkan judul lalu Enter untuk selesai.\n")
    while True:
        judul = input("Judul    : ").strip()
        if not judul:
            break
        penulis = input("Penulis  : ").strip()
        penerbit = input("Penerbit : ").strip()
        isbn = input("ISBN (boleh kosong): ").strip()
        tambah(conn, judul, penulis, penerbit, isbn, paksa=a.paksa)
        conn.commit()
        print()


def cmd_import(a):
    conn = koneksi(cari_path_db(a.db), backup=not a.no_backup)
    hitung = {"baru": 0, "update": 0, "skip": 0}
    with open(a.file, newline="", encoding="utf-8-sig") as f:
        for baris in csv.DictReader(f):
            baris = {(k or "").strip().lower(): v for k, v in baris.items()}
            hasil = tambah(
                conn, baris.get("judul"), baris.get("penulis"), baris.get("penerbit"),
                baris.get("isbn"), sumber="import_csv", paksa=a.paksa,
            )
            hitung[hasil] += 1
    conn.commit()
    laporan(hitung)


def _nilai(node):
    """Ambil nilai literal dari node AST; 'null'/'None' -> None. Tidak ada eval()."""
    if isinstance(node, ast.Name) and node.id in ("null", "None"):
        return None
    return ast.literal_eval(node)


def cmd_import_calls(a):
    """Baca file teks berisi baris tambah_ke_rag_manual("a","b","c","d")
    dengan parser AST aman (isi file TIDAK dieksekusi)."""
    conn = koneksi(cari_path_db(a.db), backup=not a.no_backup)
    hitung = {"baru": 0, "update": 0, "skip": 0}
    with open(a.file, encoding="utf-8") as f:
        for no, teks in enumerate(f, 1):
            teks = teks.strip()
            if not teks or teks.startswith("#") or teks.startswith("```"):
                continue
            try:
                node = ast.parse(teks, mode="eval").body
                if not (isinstance(node, ast.Call)
                        and getattr(node.func, "id", "") == "tambah_ke_rag_manual"):
                    raise ValueError("bukan pemanggilan tambah_ke_rag_manual")
                args = [_nilai(x) for x in node.args]
                kw = {k.arg: _nilai(k.value) for k in node.keywords}
                nama = ["judul", "penulis", "penerbit", "isbn"]
                data = dict(zip(nama, args))
                data.update({k: v for k, v in kw.items() if k in nama})
            except Exception as e:
                print(f"[SKIP] baris {no}: {e}")
                hitung["skip"] += 1
                continue
            hasil = tambah(
                conn, data.get("judul"), data.get("penulis"), data.get("penerbit"),
                data.get("isbn"), sumber="import_ai", paksa=a.paksa,
            )
            hitung[hasil] += 1
    conn.commit()
    laporan(hitung)


def cmd_list(a):
    conn = koneksi(cari_path_db(a.db), backup=False)
    q, p = "SELECT id, judul, penulis, penerbit, isbn, sumber FROM rag_manual", ()
    if a.cari:
        q += " WHERE LOWER(judul) LIKE ? OR LOWER(penulis) LIKE ? OR LOWER(penerbit) LIKE ? OR isbn LIKE ?"
        k = f"%{a.cari.lower()}%"
        p = (k, k, k, k)
    q += " ORDER BY id"
    rows = conn.execute(q, p).fetchall()
    for r in rows:
        print(f"#{r[0]:<4} {r[1]} | {r[2]} | {r[3]} | ISBN: {r[4]} | {r[5]}")
    print(f"\n{len(rows)} baris.")


def cmd_hapus(a):
    conn = koneksi(cari_path_db(a.db), backup=not a.no_backup)
    row = conn.execute("SELECT judul, penulis FROM rag_manual WHERE id=?", (a.id,)).fetchone()
    if not row:
        print(f"ID {a.id} tidak ada.")
        return
    if not a.yes and input(f"Hapus #{a.id} '{row[0]}' - {row[1]}? (y/N) ").lower() != "y":
        print("Dibatalkan.")
        return
    conn.execute("DELETE FROM rag_manual WHERE id=?", (a.id,))
    conn.commit()
    print("Terhapus.")


def cmd_cek(a):
    conn = koneksi(cari_path_db(a.db), backup=False)
    rows = conn.execute("SELECT id, judul, penulis, penerbit, isbn FROM rag_manual").fetchall()
    total = len(rows)
    isbn_ada = [r for r in rows if r[4]]
    invalid = [r for r in isbn_ada if not ean13_valid(str(r[4]))]
    tanpa_penerbit = [r for r in rows if not r[3]]
    peta = {}
    for r in isbn_ada:
        peta.setdefault(r[4], []).append(r)
    dobel = {k: v for k, v in peta.items() if len(v) > 1}

    print(f"Total buku      : {total}")
    print(f"ISBN terisi     : {len(isbn_ada)}/{total}")
    print(f"ISBN tidak valid: {len(invalid)}")
    for r in invalid:
        print(f"   #{r[0]} {r[1]} -> {r[4]}")
    print(f"Tanpa penerbit  : {len(tanpa_penerbit)}")
    for r in tanpa_penerbit:
        print(f"   #{r[0]} {r[1]}")
    print(f"ISBN dobel      : {len(dobel)}")
    for isbn, lst in dobel.items():
        print(f"   {isbn}: " + "; ".join(f"#{r[0]} {r[1]}" for r in lst))


# ---------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser(description="Kelola rag_manual dari terminal")
    ap.add_argument("--db", help="path book_cache.db (default: env RAG_DB atau ./book_cache.db)")
    ap.add_argument("--no-backup", action="store_true", help="jangan bikin file backup otomatis")
    ap.add_argument("--paksa", action="store_true", help="simpan ISBN walau checksum tidak valid")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("add", help="tambah/update satu buku (kosongkan argumen = interaktif)")
    p.add_argument("judul", nargs="?")
    p.add_argument("penulis", nargs="?", default="")
    p.add_argument("penerbit", nargs="?", default="")
    p.add_argument("isbn", nargs="?")
    p.set_defaults(fn=cmd_add)

    p = sub.add_parser("import", help="import dari CSV (kolom: judul,penulis,penerbit,isbn)")
    p.add_argument("file")
    p.set_defaults(fn=cmd_import)

    p = sub.add_parser("import-calls", help="import dari file berisi baris tambah_ke_rag_manual(...)")
    p.add_argument("file")
    p.set_defaults(fn=cmd_import_calls)

    p = sub.add_parser("list", help="lihat isi rag_manual")
    p.add_argument("--cari", help="filter judul/penulis/penerbit/isbn")
    p.set_defaults(fn=cmd_list)

    p = sub.add_parser("hapus", help="hapus satu baris berdasarkan id")
    p.add_argument("id", type=int)
    p.add_argument("-y", "--yes", action="store_true", help="tanpa konfirmasi")
    p.set_defaults(fn=cmd_hapus)

    p = sub.add_parser("cek", help="statistik + cek ISBN invalid/dobel/penerbit kosong")
    p.set_defaults(fn=cmd_cek)

    a = ap.parse_args()
    a.fn(a)


if __name__ == "__main__":
    main()
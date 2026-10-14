#!/usr/bin/env python3
"""Tes otomatis rag.exe. Jalankan dari folder tempat rag.exe berada:

    python tes_rag.py
    python tes_rag.py --salinan "C:\\path\\ke\\db_salinan.db"   (tambah tes di salinan DB asli)

Semua tes memakai DB sementara. DB yang kamu beri lewat --salinan hanya DISALIN, tidak disentuh.
Exit code 0 = semua lulus, 1 = ada yang gagal.
"""
import argparse
import glob
import os
import shutil
import sqlite3
import subprocess
import sys
import tempfile

hasil = []


def cek(nama, kondisi, detail=""):
    hasil.append((nama, bool(kondisi)))
    print(("[PASS] " if kondisi else "[FAIL] ") + nama)
    if not kondisi and detail:
        print("       " + detail.strip().replace("\n", "\n       ")[:700])


class Rag:
    def __init__(self, exe, cwd):
        self.exe, self.cwd = exe, cwd

    def jalan(self, *args, db=None):
        cmd = [self.exe] + (["--db", db] if db else []) + list(args)
        p = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8",
                           errors="replace", timeout=60, input="", cwd=self.cwd)
        return p.returncode, p.stdout, p.stderr


def backups(db):
    return [f for f in glob.glob(glob.escape(db) + ".bak-*") if not f.endswith(".tmp")]


def mode_journal(db):
    con = sqlite3.connect(db)
    try:
        return con.execute("PRAGMA journal_mode").fetchone()[0].lower()
    finally:
        con.close()


def integritas(db):
    con = sqlite3.connect(db)
    try:
        return con.execute("PRAGMA integrity_check").fetchone()[0]
    finally:
        con.close()


def hitung_baris(db):
    con = sqlite3.connect(db)
    try:
        tabel = [r[0] for r in con.execute(
            "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'")]
        return {t: con.execute(f'SELECT COUNT(*) FROM "{t}"').fetchone()[0] for t in tabel}
    finally:
        con.close()


def db_kosong(folder, nama="t.db"):
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, nama)
    open(path, "wb").close()  # rag membuka DB tanpa SQLITE_OPEN_CREATE, jadi file harus ada
    return path


# ----------------------------------------------------------------------------------------
def tes_validasi(rag, tmp):
    print("\n== Validasi bentuk argumen (tidak boleh menyentuh DB / membuat backup)")
    db = db_kosong(os.path.join(tmp, "validasi"))
    rc, _, _ = rag.jalan()
    cek("tanpa argumen -> exit 1 (cetak bantuan)", rc == 1)
    kasus = [
        (["foo"], "perintah tidak dikenal"),
        (["wal", "foo"], "aksi wal tidak dikenal"),
        (["wal", "pasang", "extra"], "tidak memakai argumen tambahan"),
        (["list", "--paksa"], "paksa"),
        (["add", "x", "--opsi-ngawur"], "opsi tidak dikenal"),
    ]
    for args, teks in kasus:
        rc, o, e = rag.jalan(*args, db=db)
        cek(f"rag {' '.join(args)} -> ditolak", rc == 1 and teks in e, e)
    cek("validasi tidak membuat backup", len(backups(db)) == 0)


def tes_crud_dan_backup(rag, tmp):
    print("\n== CRUD rag_manual dan backup")
    db = db_kosong(os.path.join(tmp, "crud"))
    rc, o, e = rag.jalan("migrasi", db=db)
    cek("migrasi di DB kosong", rc == 0 and "[OK]" in o, e)

    rc, o, e = rag.jalan("add", "Judul A", "Penulis A", "Penerbit A", db=db)
    cek("add -> [baru] + [backup]", rc == 0 and "[baru]" in o and "[backup]" in o, o + e)
    rc, o, e = rag.jalan("add", "Judul A", "Penulis A", "Penerbit B", db=db)
    cek("add judul sama -> [update], bukan duplikat", rc == 0 and "[update]" in o, o + e)

    n = len(backups(db))
    rc, o, e = rag.jalan("list", db=db)
    cek("list menampilkan data (1 baris)", rc == 0 and "Judul A" in o and "1 baris" in o, o + e)
    cek("list (baca saja) tidak membuat backup", len(backups(db)) == n)
    rc, o, _ = rag.jalan("list", "tidak-ada-xyz", db=db)
    cek("list kata yang tidak ada -> 0 baris", "0 baris" in o)

    rc, o, e = rag.jalan("add", "Judul NB", "P", "Pen", "--no-backup", db=db)
    cek("--no-backup tidak membuat backup baru", rc == 0 and len(backups(db)) == n, o + e)

    for i in range(6):
        rag.jalan("add", f"Rotasi {i}", "P", "Pen", "--simpan-backup", "3", db=db)
    cek("rotasi: --simpan-backup 3 menyisakan tepat 3 backup", len(backups(db)) == 3,
        f"jumlah backup: {len(backups(db))}")

    rc, o, e = rag.jalan("hapus", "1", "-y", db=db)
    cek("hapus 1 -y", rc == 0 and "Terhapus (1 baris)" in o, o + e)
    rc, o, _ = rag.jalan("hapus", "99999", "-y", db=db)
    cek("hapus id yang tidak ada -> pesan, bukan crash", rc == 0 and "tidak ada" in o)
    cek("integrity_check ok", integritas(db) == "ok")


def tes_cache(rag, tmp):
    print("\n== Cache metadata")
    db = db_kosong(os.path.join(tmp, "cache"))
    key = "judul_c_penulis_c"
    rc, o, e = rag.jalan("cache", "add", "Judul C", "Penulis C", "Penerbit C", db=db)
    cek("cache add -> [baru] dengan key yang benar", rc == 0 and "[baru]" in o and key in o, o + e)
    rc, o, e = rag.jalan("cache", "add", "Judul C", "Penulis C", "Penerbit C", db=db)
    cek("cache add ulang -> [update]", rc == 0 and "[update]" in o, o + e)
    rc, o, e = rag.jalan("cache", "list", db=db)
    cek("cache list memuat entri", rc == 0 and (key in o or "Judul C" in o), o + e)
    rc, o, e = rag.jalan("cache", "status", key, "ditinjau", db=db)
    cek("cache status -> ditinjau", rc == 0 and "ditinjau" in o, o + e)
    rc, o, e = rag.jalan("cache", "status", key, "dipromosikan", db=db)
    cek("status 'dipromosikan' ditolak (hanya lewat promosi)", rc == 1 and "promosi" in e, e)
    rc, o, e = rag.jalan("cache", "hapus", key, "-y", db=db)
    cek("cache hapus -y", rc == 0 and "Terhapus (1 baris)" in o, o + e)
    cek("integrity_check ok", integritas(db) == "ok")


def tes_wal(rag, tmp, salinan=None):
    print("\n== WAL")
    if salinan:
        db = os.path.join(tmp, "wal", "t.db")
        os.makedirs(os.path.dirname(db), exist_ok=True)
        shutil.copy2(salinan, db)
        print("   (memakai salinan DB-mu)")
        if mode_journal(db) == "wal":  # salinan sudah WAL: kembalikan dulu supaya tes mulai dari delete
            rag.jalan("wal", "lepas", "--no-backup", db=db)
    else:
        db = db_kosong(os.path.join(tmp, "wal"))
        rag.jalan("add", "Judul W", "Penulis W", "Penerbit W", db=db)
    sebelum = hitung_baris(db)

    rc, o, e = rag.jalan("wal", db=db)
    cek("status awal: journal_mode delete", rc == 0 and "journal_mode   : delete" in o, o + e)
    n = len(backups(db))

    rc, o, e = rag.jalan("wal", "pasang", db=db)
    cek("wal pasang -> OK", rc == 0 and "journal_mode=WAL aktif" in o, o + e)
    cek("journal_mode di file DB = wal", mode_journal(db) == "wal")
    cek("wal pasang membuat backup", len(backups(db)) > n)

    n = len(backups(db))
    rc, o, e = rag.jalan("wal", "pasang", db=db)
    cek("wal pasang ulang -> 'sudah WAL', tanpa backup baru",
        rc == 0 and "sudah WAL" in o and len(backups(db)) == n, o + e)

    rc, o, e = rag.jalan("wal", db=db)
    cek("status: wal + synchronous NORMAL", "journal_mode   : wal" in o and "1 (NORMAL)" in o, o + e)
    rc, o, e = rag.jalan("add", "Judul W2", "Penulis W2", "Penerbit W2", db=db)
    cek("tulis di mode WAL berjalan", rc == 0 and "[baru]" in o, o + e)
    rc, o, e = rag.jalan("list", db=db)
    cek("baca di mode WAL berjalan", rc == 0 and "Judul W2" in o, o + e)

    rc, o, e = rag.jalan("wal", "lepas", db=db)
    cek("wal lepas -> DELETE + synchronous FULL",
        rc == 0 and "journal_mode=DELETE; synchronous=FULL" in o, o + e)
    cek("journal_mode di file DB = delete", mode_journal(db) == "delete")
    cek("tidak ada sisa file -wal / -shm",
        not os.path.exists(db + "-wal") and not os.path.exists(db + "-shm"))
    sesudah = hitung_baris(db)
    ditambah = 1  
    cek("jumlah baris semua tabel utuh setelah pasang->lepas",
        all(sesudah.get(t) == v + (ditambah if t == "rag_manual" else 0)
            for t, v in sebelum.items()), f"sebelum={sebelum} sesudah={sesudah}")
    cek("integrity_check ok", integritas(db) == "ok")
    rc, o, e = rag.jalan("wal", "lepas", db=db)
    cek("wal lepas ulang -> 'tidak sedang WAL'", rc == 0 and "tidak sedang WAL" in o, o + e)


def tes_wal_lokasi_berisiko(rag, tmp):
    print("\n== WAL: penolakan lokasi sinkronisasi (folder bernama OneDrive)")
    db = db_kosong(os.path.join(tmp, "OneDrive", "proyek"), "x.db")
    rag.jalan("add", "Judul O", "Penulis O", "Penerbit O", db=db)
    n = len(backups(db))

    rc, o, e = rag.jalan("wal", db=db)
    cek("status melaporkan BERISIKO", "BERISIKO" in o, o + e)
    rc, o, e = rag.jalan("wal", "pasang", db=db)
    cek("wal pasang ditolak", rc == 1 and "WAL tidak dipasang" in e, o + e)
    cek("penolakan tidak membuat backup", len(backups(db)) == n)
    cek("DB tetap mode delete", mode_journal(db) == "delete")

    rc, o, e = rag.jalan("wal", "pasang", "--paksa", db=db)
    cek("wal pasang --paksa lolos", rc == 0 and "WAL aktif" in o, o + e)
    rc, o, e = rag.jalan("wal", "lepas", db=db)
    cek("kembalikan ke delete", rc == 0 and mode_journal(db) == "delete", o + e)


# ----------------------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rag", help="path rag.exe (bawaan: rag.exe di folder ini)")
    ap.add_argument("--salinan", help="path DB salinan untuk tes tambahan (hanya disalin)")
    ap.add_argument("--tmp", help="folder kerja sementara (bawaan: temp sistem)")
    ap.add_argument("--simpan-tmp", action="store_true", help="jangan hapus folder sementara")
    a = ap.parse_args()

    exe = os.path.abspath(a.rag or ("rag.exe" if os.path.exists("rag.exe") else "rag"))
    if not os.path.exists(exe):
        print(f"rag.exe tidak ditemukan di {exe}. Jalankan build.bat dulu, atau pakai --rag <path>.")
        return 2
    if a.salinan and not os.path.exists(a.salinan):
        print(f"DB salinan tidak ada: {a.salinan}")
        return 2

    tmp = tempfile.mkdtemp(prefix="ragtes_", dir=a.tmp)
    print(f"rag     : {exe}\nfolder  : {tmp}")
    rag = Rag(exe, tmp)
    try:
        tes_validasi(rag, tmp)
        tes_crud_dan_backup(rag, tmp)
        tes_cache(rag, tmp)
        tes_wal(rag, tmp)
        tes_wal_lokasi_berisiko(rag, tmp)
        if a.salinan:
            tes_wal(rag, os.path.join(tmp, "salinan"), a.salinan)
    finally:
        if not a.simpan_tmp:
            shutil.rmtree(tmp, ignore_errors=True)

    gagal = [n for n, ok in hasil if not ok]
    print(f"\n{len(hasil) - len(gagal)}/{len(hasil)} lulus.")
    if gagal:
        print("GAGAL:")
        for n in gagal:
            print("  - " + n)
    return 1 if gagal else 0


if __name__ == "__main__":
    sys.exit(main())
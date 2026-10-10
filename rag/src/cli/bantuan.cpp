#include "bantuan.h"

#include <iostream>

void cetak_bantuan() {
    std::cerr <<
        "Pakai: rag [--db path] add <judul> <penulis> <penerbit> [isbn]\n"
        "       rag [--db path] import file.csv [--tanpa-transaksi] [--rinci]\n"
        "       rag [--db path] list [kata]\n"
        "       rag [--db path] edit <id> <judul|penulis|penerbit|isbn> <nilai>\n"
        "       rag [--db path] hapus <id> [-y]\n"
        "\n"
        "       rag [--db path] cache list [baru|ditinjau|dipromosikan|ditolak] [kata]\n"
        "       rag [--db path] cache status <key> <baru|ditinjau|ditolak>\n"
        "       rag [--db path] cache hapus <key> [-y]\n"
        "       rag [--db path] cache bersihkan [--hari N] [-y]\n"
        "       rag [--db path] cache promosi <key> [penerbit] [isbn]\n"
        "       rag [--db path] cache promosi --otomatis [--cek] [--rinci]\n"                       // <-- BARU
        "                       (naikkan semua entri cache yang lengkap; tidak pernah menimpa\n"     // <-- BARU
        "                        rag_manual. --cek = lihat saja, tidak menulis apa pun)\n"          // <-- BARU
        "       rag [--db path] cache add <judul> <penulis> <penerbit> [isbn]\n"
        "\n"
        "       rag [--db path] cache scan list [kata]\n"
        "       rag [--db path] cache scan get <awalan-hash>\n"
        "       rag [--db path] cache scan add <hash> <judul> <penulis> [kategori] [isbn]\n"
        "       rag [--db path] cache scan isbn <isbn>\n"
        "       rag [--db path] cache scan hapus <awalan-hash> [-y]\n"
        "       rag [--db path] cache scan bersihkan --hari N [-y]\n"
        "\n"
        "       rag [--db path] katalog list [otomatis|terkonfirmasi] [kata]\n"
        "       rag [--db path] katalog get <id>\n"
        "       rag [--db path] katalog konfirmasi <id> [penerbit] [isbn] [stok] [-y]\n"
        "                       (\"-\" atau kosong = tidak diubah)\n"
        "\n"
        "       rag [--db path] cari <judul> [penulis]   (telusuri cache_metadata, lalu rag_manual)\n"
        "       rag [--db path] cari-isbn <isbn>         (telusuri lewat ISBN)\n"
        "\n"
        "       rag [--db path] migrasi    (backup dulu, lalu mutakhirkan skema semua tabel)\n"
        "\n"
        "Opsi:\n"
        "  --db path           pakai file DB ini (bawaan: book_cache.db, atau variabel RAG_DB)\n"
        "  --hari N            batas umur dalam hari (cache bersihkan, cache scan bersihkan)\n"
        "  --simpan-backup N   simpan N backup terakhir (bawaan 5, minimal 1)\n"
        "  --no-backup         jangan buat backup sebelum perintah yang menulis\n"
        "  --tanpa-transaksi   import tanpa satu transaksi besar\n"
        "  --rinci             tampilkan keterangan per baris (import, cache promosi --otomatis)\n"
        "  -y                  lewati pertanyaan konfirmasi\n"
        "  --otomatis          hanya untuk cache promosi: naikkan semua entri yang lengkap\n"
        "  --cek               hanya bersama --otomatis: lihat saja, tidak menulis apa pun\n";
}
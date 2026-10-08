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
        "       rag [--db path] cache add <judul> <penulis> <penerbit> [isbn]\n"
        "\n"
        "       rag [--db path] cache scan list [kata]\n"
        "       rag [--db path] cache scan get <awalan-hash>\n"
        "       rag [--db path] cache scan add <hash> <judul> <penulis> [kategori]\n"
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
        "Opsi: --no-backup\n";
}
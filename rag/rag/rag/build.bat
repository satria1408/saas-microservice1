@echo off
clang++ -std=c++17 -O2 -Wall -Wextra -Isrc -Isrc/cli ^
  src/main.cpp src/db.cpp src/util.cpp src/csv.cpp ^
  src/cli/opsi.cpp src/cli/validasi.cpp src/cli/bantuan.cpp ^
  src/cli/middleware.cpp src/cli/router.cpp ^
  src/cli/kontrol/rag_kontrol.cpp src/cli/kontrol/cari_kontrol.cpp ^
  src/cli/kontrol/migrasi_kontrol.cpp src/cli/kontrol/katalog_kontrol.cpp ^
  src/cli/kontrol/cache_kontrol.cpp src/cli/kontrol/scan_kontrol.cpp ^
  src/crud/create.cpp src/crud/read.cpp src/crud/update.cpp src/crud/delete.cpp ^
  src/cache/metadata/meta_create.cpp src/cache/metadata/meta_read.cpp ^
  src/cache/metadata/meta_kelola.cpp src/cache/metadata/meta_promosi.cpp ^
  src/cache/metadata/meta_ttl.cpp ^
  src/cache/scan/scan_skema.cpp src/cache/scan/scan_create.cpp ^
  src/cache/scan/scan_read.cpp src/cache/scan/scan_hapus.cpp ^
  src/cache/scan/scan_bersihkan.cpp ^
  src/katalog/katalog_skema.cpp src/katalog/katalog_read.cpp ^
  src/katalog/katalog_ubah.cpp ^
  src/alur/alur_cari.cpp src/alur/kemiripan.cpp ^
  -o rag.exe -lsqlite3
if %errorlevel%==0 (echo Build OK: rag.exe) else (echo Build GAGAL)
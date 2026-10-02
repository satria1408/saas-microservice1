@echo off
clang++ -std=c++17 -O2 -Wall -Wextra -Isrc ^
  src/main.cpp src/db.cpp src/util.cpp src/csv.cpp ^
  src/crud/create.cpp src/crud/read.cpp src/crud/update.cpp src/crud/delete.cpp ^
  src/cache/metadata/meta_create.cpp src/cache/metadata/meta_read.cpp ^
  src/cache/metadata/meta_kelola.cpp src/cache/metadata/meta_promosi.cpp ^
  -o rag.exe -lsqlite3
if %errorlevel%==0 (echo Build OK: rag.exe) else (echo Build GAGAL)
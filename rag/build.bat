@echo off
REM Build tanpa make (untuk CMD/PowerShell). Jalankan: build.bat
g++ -std=c++17 -O2 -Wall -Wextra src\main.cpp src\db.cpp src\util.cpp src\csv.cpp src\create.cpp src\read.cpp src\update.cpp src\delete.cpp -o rag.exe -lsqlite3
if %errorlevel%==0 (echo Build OK: rag.exe) else (echo Build GAGAL)

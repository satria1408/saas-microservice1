@echo off
cd /d "%~dp0"
if exist rag.exe del rag.exe
call build.bat
if not exist rag.exe (
  echo Build gagal, tes dibatalkan.
  exit /b 1
)
python tes_rag.py %*
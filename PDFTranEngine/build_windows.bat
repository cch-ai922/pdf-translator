@echo off
REM Build PDFTranEngine.exe with PyInstaller (Windows)
REM Prerequisites:
REM   pip install pyinstaller
REM   pip install -r requirements.txt

setlocal

echo === Installing Python dependencies ===
pip install -r requirements.txt
if errorlevel 1 (
    echo ERROR: pip install failed
    exit /b 1
)

echo === Building PDFTranEngine.exe ===
pyinstaller ^
    --onefile ^
    --console ^
    --name PDFTranEngine ^
    --hidden-import pdf2docx ^
    --hidden-import pdfplumber ^
    --hidden-import pdf2image ^
    --hidden-import pytesseract ^
    --hidden-import docx ^
    --hidden-import PIL ^
    main.py

if errorlevel 1 (
    echo ERROR: PyInstaller failed
    exit /b 1
)

echo.
echo === Done ===
echo Output: dist\PDFTranEngine.exe
echo Copy dist\PDFTranEngine.exe next to PDFTranView.exe
endlocal

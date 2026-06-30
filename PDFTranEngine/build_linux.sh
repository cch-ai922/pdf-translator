#!/usr/bin/env bash
# Build PDFTranEngine (Linux) with PyInstaller
# Prerequisites:
#   pip3 install pyinstaller
#   pip3 install -r requirements.txt
#   sudo apt install tesseract-ocr poppler-utils libreoffice

set -e

echo "=== Installing Python dependencies ==="
pip3 install -r requirements.txt

echo "=== Building PDFTranEngine ==="
pyinstaller \
    --onefile \
    --console \
    --name PDFTranEngine \
    --hidden-import pdf2docx \
    --hidden-import pdfplumber \
    --hidden-import pdf2image \
    --hidden-import pytesseract \
    --hidden-import docx \
    --hidden-import PIL \
    main.py

echo ""
echo "=== Done ==="
echo "Output: dist/PDFTranEngine"
echo "Copy dist/PDFTranEngine next to PDFTranView"

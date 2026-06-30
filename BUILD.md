# PDF Translator – Build Guide

## Project layout

```
pdf-translator/
├── CMakeLists.txt          root CMake (builds PDFTranView)
├── PDFTranView/            Qt5 GUI application (C++)
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── mainwindow.h/cpp    main window + menus
│   ├── pdfdocument.h/cpp   MuPDF wrapper (open/render)
│   └── pdfview.h/cpp       scrollable, zoomable PDF widget
└── PDFTranEngine/          Console converter (Python → .exe)
    ├── main.py             PDF→DOCX→PDF pipeline
    ├── requirements.txt
    ├── build_windows.bat
    └── build_linux.sh
```

---

## Prerequisites

### Common
| Software | Where to get |
|----------|--------------|
| Qt 5.5 (or later 5.x) | https://www.qt.io/offline-installers |
| CMake ≥ 3.5 | https://cmake.org/download/ |
| MuPDF (static lib + headers) | https://mupdf.com/downloads/ |
| Python ≥ 3.8 | https://www.python.org/ |
| LibreOffice | https://www.libreoffice.org/ |
| Tesseract-OCR (optional, for scanned PDFs) | https://github.com/tesseract-ocr/tesseract |
| Poppler utils (for pdf2image) | https://poppler.freedesktop.org/ |

---

## Build PDFTranView (Qt5 / C++)

### Windows – MinGW

```cmd
mkdir build && cd build
cmake .. -G "MinGW Makefiles" ^
         -DCMAKE_PREFIX_PATH=C:/Qt/5.15.2/mingw81_64 ^
         -DMUPDF_DIR=C:/mupdf
mingw32-make -j4
```

### Windows – MSVC

```cmd
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64 ^
         -DCMAKE_PREFIX_PATH=C:/Qt/5.15.2/msvc2019_64 ^
         -DMUPDF_DIR=C:/mupdf
cmake --build . --config Release
```

### Linux (GCC)

```bash
mkdir build && cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=/opt/Qt/5.15.2/gcc_64
make -j$(nproc)
```

> **MuPDF note** – On Linux you can usually `sudo apt install libmupdf-dev`.
> On Windows download the pre-built static library from mupdf.com, then
> point `-DMUPDF_DIR` at the extraction folder.

---

## Build PDFTranEngine (Python → .exe)

### Windows

```cmd
cd PDFTranEngine
build_windows.bat
```

Output: `PDFTranEngine/dist/PDFTranEngine.exe`

### Linux

```bash
cd PDFTranEngine
chmod +x build_linux.sh
./build_linux.sh
```

Output: `PDFTranEngine/dist/PDFTranEngine`

---

## Deployment

Copy both executables to the same folder:

```
deploy/
├── PDFTranView.exe      (or PDFTranView on Linux)
└── PDFTranEngine.exe    (or PDFTranEngine on Linux)
```

PDFTranView looks for PDFTranEngine in its own directory.

### Windows – Qt runtime DLLs

Run `windeployqt PDFTranView.exe` from a Qt command prompt to copy
the required Qt DLLs next to the executable.

---

## Adding real translation to PDFTranEngine

Edit `PDFTranEngine/main.py` and replace the body of `translate_docx()`.

Offline translation options to consider:

| Tool | Notes |
|------|-------|
| **LibreTranslate** (local server) | REST API, supports many languages |
| **CTranslate2** + Helsinki-NLP OPUS-MT | Fast CPU inference, many language pairs |
| **Ollama** + local LLM | High quality, requires GPU for speed |
| **Argos Translate** | Pure Python, offline, 100+ language pairs |

Example (Argos Translate):

```python
import argostranslate.package, argostranslate.translate
from docx import Document

def translate_docx(docx_path, src_lang, tgt_lang):
    # ... install language pack if needed, then:
    doc = Document(docx_path)
    for para in doc.paragraphs:
        if para.text.strip():
            para.text = argostranslate.translate.translate(para.text, src_lang, tgt_lang)
    doc.save(docx_path)
```

---

## Tesseract language packs

For OCR of Chinese/Japanese/Korean PDFs, download the corresponding
`.traineddata` files from https://github.com/tesseract-ocr/tessdata
and place them in your Tesseract `tessdata/` directory:
- `chi_sim.traineddata`  – Simplified Chinese
- `chi_tra.traineddata`  – Traditional Chinese
- `jpn.traineddata`      – Japanese
- `kor.traineddata`      – Korean

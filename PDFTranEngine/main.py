"""
PDFTranEngine – offline PDF -> DOCX -> PDF pipeline.

Usage:
    PDFTranEngine.exe <source.pdf> [src_lang] [tgt_lang]

Produces <source_tran.pdf> in the same directory as <source.pdf>.

Conversion strategy
───────────────────
Text-based PDFs  -> pdf2docx  (layout-preserving, very fast, no OCR needed)
Image-based PDFs -> pdf2image + pytesseract OCR -> python-docx -> DOCX
DOCX -> PDF       -> mammoth (DOCX->HTML) + weasyprint (HTML->PDF)
                   Falls back to LibreOffice headless if installed.

All converters work fully offline once the Python packages are installed.
No admin rights required.
"""

import sys
import os
import shutil
import tempfile

# Force UTF-8 stdout/stderr on Windows so non-ASCII log chars don't crash
if sys.stdout and hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if sys.stderr and hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

# When running as a PyInstaller onefile bundle, Pango/Cairo DLLs are extracted
# to a subfolder named weasyprint_dlls. Add it to PATH before importing
# weasyprint so cffi can locate them via the OS loader.
if getattr(sys, "frozen", False):
    _dll_dir = os.path.join(sys._MEIPASS, "weasyprint_dlls")  # noqa: SLF001
    if os.path.isdir(_dll_dir):
        os.environ["PATH"] = _dll_dir + os.pathsep + os.environ.get("PATH", "")
        # Also try the Windows 8+ AddDllDirectory API
        try:
            import ctypes
            ctypes.windll.kernel32.AddDllDirectory(_dll_dir)
        except Exception:
            pass

# ── Helpers ──────────────────────────────────────────────────────────────────

def log(msg: str) -> None:
    print(msg, flush=True)


def die(msg: str, code: int = 1) -> None:
    print(f"ERROR: {msg}", file=sys.stderr, flush=True)
    sys.exit(code)


def is_image_based_pdf(pdf_path: str) -> bool:
    """Returns True when the PDF has no extractable text (likely scanned)."""
    try:
        import pdfplumber
        with pdfplumber.open(pdf_path) as pdf:
            for page in pdf.pages[:min(3, len(pdf.pages))]:
                if (page.extract_text() or "").strip():
                    return False
        return True
    except Exception:
        return False  # assume text-based on error


# ── PDF -> DOCX (text-based) ──────────────────────────────────────────────────

def pdf_to_docx_text(pdf_path: str, docx_path: str) -> None:
    """Convert a text-based PDF to DOCX using pdf2docx."""
    log(f"[text] pdf2docx: {pdf_path} -> {docx_path}")
    from pdf2docx import Converter
    cv = Converter(pdf_path)
    cv.convert(docx_path, start=0, end=None)
    cv.close()
    log("[text] pdf2docx done")


# ── PDF -> DOCX (image-based, with OCR) ───────────────────────────────────────

def pdf_to_docx_ocr(pdf_path: str, docx_path: str, lang: str = "chi_sim+eng") -> None:
    """Convert a scanned PDF to DOCX via Tesseract OCR."""
    log(f"[ocr] Starting OCR pipeline: lang={lang}")
    try:
        from pdf2image import convert_from_path
        import pytesseract
        from docx import Document
        from docx.shared import Inches
    except ImportError as e:
        die(f"OCR dependencies missing: {e}\n"
            "Install: pip install pdf2image pytesseract python-docx pillow\n"
            "Also install Tesseract-OCR (https://github.com/tesseract-ocr/tesseract)")

    with tempfile.TemporaryDirectory() as tmp:
        log("[ocr] Rasterising PDF pages...")
        pages = convert_from_path(pdf_path, dpi=300, output_folder=tmp,
                                  fmt="png", thread_count=2)
        log(f"[ocr] {len(pages)} page(s) rasterised, running Tesseract...")

        doc = Document()
        for section in doc.sections:
            section.left_margin  = Inches(0.75)
            section.right_margin = Inches(0.75)

        for i, img in enumerate(pages):
            log(f"[ocr] OCR page {i+1}/{len(pages)}")
            text = pytesseract.image_to_string(img, lang=lang)
            if i > 0:
                doc.add_page_break()
            doc.add_paragraph(text)

        doc.save(docx_path)
    log("[ocr] OCR DOCX saved")


# ── DOCX -> PDF ────────────────────────────────────────────────────────────────

def docx_to_pdf_weasyprint(docx_path: str, pdf_path: str) -> None:
    """
    Convert DOCX -> PDF using mammoth (DOCX->HTML) + weasyprint (HTML->PDF).
    Pure Python, no admin rights, no external tools required.
    """
    log("[docx2pdf] Using mammoth + weasyprint")
    try:
        import mammoth
        from weasyprint import HTML, CSS
    except ImportError as e:
        die(f"mammoth/weasyprint missing: {e}\n"
            "Install: pip install mammoth weasyprint")

    # Step A: DOCX -> HTML
    with open(docx_path, "rb") as f:
        result = mammoth.convert_to_html(f)
    html_body = result.value

    # Wrap with basic page CSS for a clean PDF layout
    html_full = f"""<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8"/>
<style>
  @page {{ margin: 2cm; }}
  body {{
    font-family: "Arial Unicode MS", "Noto Sans CJK SC", Arial, sans-serif;
    font-size: 11pt;
    line-height: 1.5;
    color: #000;
  }}
  p  {{ margin: 0 0 0.4em 0; }}
  h1 {{ font-size: 18pt; margin: 0.8em 0 0.4em 0; }}
  h2 {{ font-size: 14pt; margin: 0.7em 0 0.3em 0; }}
  h3 {{ font-size: 12pt; margin: 0.6em 0 0.3em 0; }}
  table {{ border-collapse: collapse; width: 100%; margin: 0.5em 0; }}
  td, th {{ border: 1px solid #ccc; padding: 4px 8px; }}
  img {{ max-width: 100%; }}
</style>
</head>
<body>{html_body}</body>
</html>"""

    # Step B: HTML -> PDF
    HTML(string=html_full).write_pdf(pdf_path)
    log(f"[docx2pdf] weasyprint done -> {pdf_path}")


def docx_to_pdf_libreoffice(docx_path: str, out_dir: str) -> str:
    """
    Fallback: convert DOCX -> PDF via LibreOffice headless.
    Returns the output PDF path.
    """
    import subprocess
    import glob as _glob

    candidates = [
        "soffice",
        r"C:\Program Files\LibreOffice\program\soffice.exe",
        r"C:\Program Files (x86)\LibreOffice\program\soffice.exe",
        os.path.expanduser(r"~\LibreOffice\program\soffice.exe"),
        os.path.expandvars(r"%LOCALAPPDATA%\LibreOffice\program\soffice.exe"),
        *_glob.glob(os.path.expanduser(
            r"~\AppData\Local\LibreOffice*\program\soffice.exe")),
        "/usr/bin/soffice",
        "/usr/lib/libreoffice/program/soffice",
        "/opt/libreoffice/program/soffice",
    ]
    soffice = next((c for c in candidates
                    if shutil.which(c) or os.path.isfile(c)), None)
    if not soffice:
        return None  # signal: not available

    log(f"[docx2pdf] LibreOffice fallback: {soffice}")
    result = subprocess.run(
        [soffice, "--headless", "--norestore",
         "--convert-to", "pdf", "--outdir", out_dir, docx_path],
        capture_output=True, text=True, timeout=300
    )
    if result.returncode != 0:
        log(f"[docx2pdf] LibreOffice failed: {result.stderr}")
        return None

    base = os.path.splitext(os.path.basename(docx_path))[0]
    out  = os.path.join(out_dir, base + ".pdf")
    return out if os.path.isfile(out) else None


def docx_to_pdf(docx_path: str, out_dir: str) -> str:
    """
    Convert DOCX -> PDF.  Tries mammoth+weasyprint first (no admin, pure Python),
    falls back to LibreOffice if installed.
    Returns the path of the produced PDF.
    """
    # Primary: weasyprint (always available via pip)
    pdf_path = os.path.join(out_dir,
                            os.path.splitext(
                                os.path.basename(docx_path))[0] + ".pdf")
    try:
        docx_to_pdf_weasyprint(docx_path, pdf_path)
        if os.path.isfile(pdf_path):
            return pdf_path
    except Exception as e:
        log(f"[docx2pdf] weasyprint failed ({e}), trying LibreOffice...")

    # Fallback: LibreOffice
    lo_pdf = docx_to_pdf_libreoffice(docx_path, out_dir)
    if lo_pdf:
        return lo_pdf

    die("DOCX->PDF conversion failed. "
        "Install LibreOffice or fix weasyprint:\n"
        "  pip install mammoth weasyprint")


# ── Translation stub ──────────────────────────────────────────────────────────

def translate_docx(docx_path: str, src_lang: str, tgt_lang: str) -> None:
    """
    Placeholder – translate the DOCX in-place.

    Replace this body with real translation logic, e.g.:
      - Argos Translate (offline, pip install argostranslate)
      - CTranslate2 + Helsinki-NLP OPUS-MT model
      - Ollama with a local LLM
      - DeepL / OpenAI API (requires internet)
    """
    log(f"[translate] stub: {src_lang} -> {tgt_lang} (no-op for now)")


# ── Main ──────────────────────────────────────────────────────────────────────

def main() -> None:
    if len(sys.argv) < 2:
        die("Usage: PDFTranEngine <source.pdf> [src_lang] [tgt_lang]")

    src_pdf  = os.path.abspath(sys.argv[1])
    src_lang = sys.argv[2] if len(sys.argv) > 2 else "Chinese"
    tgt_lang = sys.argv[3] if len(sys.argv) > 3 else "English"

    if not os.path.isfile(src_pdf):
        die(f"File not found: {src_pdf}")

    src_dir  = os.path.dirname(src_pdf)
    src_base = os.path.splitext(os.path.basename(src_pdf))[0]
    out_pdf  = os.path.join(src_dir, src_base + "_tran.pdf")

    log(f"Source : {src_pdf}")
    log(f"Output : {out_pdf}")
    log(f"Langs  : {src_lang} -> {tgt_lang}")

    with tempfile.TemporaryDirectory() as tmp:
        docx_path = os.path.join(tmp, src_base + ".docx")

        # Step 1: PDF -> DOCX
        if is_image_based_pdf(src_pdf):
            log("[pipeline] Detected image-based PDF – using OCR")
            lang_map = {
                "chinese": "chi_sim", "english": "eng", "japanese": "jpn",
                "korean":  "kor",     "french":  "fra", "german":  "deu",
                "spanish": "spa",     "arabic":  "ara", "russian": "rus",
            }
            tess_lang = lang_map.get(src_lang.lower(), "eng") + "+eng"
            pdf_to_docx_ocr(src_pdf, docx_path, lang=tess_lang)
        else:
            log("[pipeline] Detected text-based PDF – using pdf2docx")
            pdf_to_docx_text(src_pdf, docx_path)

        # Step 2: Translate (stub – no-op)
        translate_docx(docx_path, src_lang, tgt_lang)

        # Step 3: DOCX -> PDF
        converted = docx_to_pdf(docx_path, tmp)
        shutil.move(converted, out_pdf)

    log(f"[done] Output: {out_pdf}")


if __name__ == "__main__":
    main()

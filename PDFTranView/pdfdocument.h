#pragma once

#include <QString>
#include <QImage>
#include <QSizeF>

// Forward-declare MuPDF types so callers don't need mupdf headers.
struct fz_context;
struct fz_document;

class PDFDocument
{
public:
    PDFDocument();
    ~PDFDocument();

    bool open(const QString &path);
    void close();
    bool isOpen() const;

    int pageCount() const;

    // Returns the page size in points (1 pt = 1/72 inch) at zoom 1.0
    QSizeF pageSize(int pageIndex) const;

    // Render one page at the given zoom factor (1.0 = 72 dpi, 2.0 = 144 dpi …)
    // Returns a null QImage on error.
    QImage renderPage(int pageIndex, float zoom) const;

    QString filePath() const { return m_path; }

private:
    fz_context  *m_ctx  = nullptr;
    fz_document *m_doc  = nullptr;
    QString      m_path;
    int          m_pageCount = 0;

    Q_DISABLE_COPY(PDFDocument)
};

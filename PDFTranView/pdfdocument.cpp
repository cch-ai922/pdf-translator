#include "pdfdocument.h"

#include <mupdf/fitz.h>

#include <QDebug>

PDFDocument::PDFDocument() = default;

PDFDocument::~PDFDocument()
{
    close();
}

bool PDFDocument::open(const QString &path)
{
    close();

    m_ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    if (!m_ctx) {
        qWarning() << "PDFDocument: failed to create fz_context";
        return false;
    }

    fz_register_document_handlers(m_ctx);

    fz_try(m_ctx) {
        QByteArray utf8 = path.toUtf8();
        m_doc = fz_open_document(m_ctx, utf8.constData());
        m_pageCount = fz_count_pages(m_ctx, m_doc);
    }
    fz_catch(m_ctx) {
        qWarning() << "PDFDocument: cannot open" << path
                   << "–" << fz_caught_message(m_ctx);
        fz_drop_context(m_ctx);
        m_ctx = nullptr;
        return false;
    }

    m_path = path;
    return true;
}

void PDFDocument::close()
{
    if (m_doc) {
        fz_drop_document(m_ctx, m_doc);
        m_doc = nullptr;
    }
    if (m_ctx) {
        fz_drop_context(m_ctx);
        m_ctx = nullptr;
    }
    m_pageCount = 0;
    m_path.clear();
}

bool PDFDocument::isOpen() const
{
    return m_doc != nullptr;
}

int PDFDocument::pageCount() const
{
    return m_pageCount;
}

QSizeF PDFDocument::pageSize(int pageIndex) const
{
    if (!m_doc || pageIndex < 0 || pageIndex >= m_pageCount)
        return {};

    QSizeF size;
    fz_try(m_ctx) {
        fz_rect bounds;
        fz_page *page = fz_load_page(m_ctx, m_doc, pageIndex);
        bounds = fz_bound_page(m_ctx, page);
        fz_drop_page(m_ctx, page);
        size = QSizeF(bounds.x1 - bounds.x0, bounds.y1 - bounds.y0);
    }
    fz_catch(m_ctx) {
        qWarning() << "PDFDocument::pageSize error:" << fz_caught_message(m_ctx);
    }
    return size;
}

QImage PDFDocument::renderPage(int pageIndex, float zoom) const
{
    if (!m_doc || pageIndex < 0 || pageIndex >= m_pageCount)
        return {};

    QImage result;
    fz_try(m_ctx) {
        fz_matrix ctm = fz_scale(zoom, zoom);
        fz_page  *page = fz_load_page(m_ctx, m_doc, pageIndex);
        fz_rect   bounds = fz_bound_page(m_ctx, page);
        fz_irect  bbox   = fz_round_rect(fz_transform_rect(bounds, ctm));

        fz_colorspace *cs      = fz_device_rgb(m_ctx);
        fz_pixmap     *pixmap  = fz_new_pixmap_with_bbox(m_ctx, cs, bbox, nullptr, 0);
        fz_clear_pixmap_with_value(m_ctx, pixmap, 0xff); // white background

        fz_device *dev = fz_new_draw_device(m_ctx, ctm, pixmap);
        fz_run_page(m_ctx, page, dev, fz_identity, nullptr);
        fz_close_device(m_ctx, dev);
        fz_drop_device(m_ctx, dev);
        fz_drop_page(m_ctx, page);

        // pixmap samples: n=3 (RGB) each row has w*n bytes (stride = w*n)
        int w = fz_pixmap_width(m_ctx, pixmap);
        int h = fz_pixmap_height(m_ctx, pixmap);
        int n = fz_pixmap_components(m_ctx, pixmap);

        QImage::Format fmt = (n == 4) ? QImage::Format_RGBA8888 : QImage::Format_RGB888;
        // Make a deep copy because pixmap will be freed
        result = QImage(fz_pixmap_samples(m_ctx, pixmap),
                        w, h, w * n, fmt).copy();

        fz_drop_pixmap(m_ctx, pixmap);
    }
    fz_catch(m_ctx) {
        qWarning() << "PDFDocument::renderPage error:" << fz_caught_message(m_ctx);
    }
    return result;
}

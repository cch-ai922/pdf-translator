#include "pdfview.h"
#include "pdfdocument.h"

#include <mupdf/fitz.h>

#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QApplication>
#include <QtMath>

PDFView::PDFView(QWidget *parent)
    : QAbstractScrollArea(parent)
{
    setBackgroundRole(QPalette::Dark);
    setAutoFillBackground(true);
    verticalScrollBar()->setSingleStep(20);
    horizontalScrollBar()->setSingleStep(20);
}

// ── Document ────────────────────────────────────────────────────────────────

void PDFView::setDocument(PDFDocument *doc)
{
    m_doc = doc;
    m_searchText.clear();
    m_searchPage = -1;
    rebuildLayout();
    if (m_doc && m_doc->isOpen())
        verticalScrollBar()->setValue(0);
    viewport()->update();
}

// ── Navigation ──────────────────────────────────────────────────────────────

void PDFView::goToPage(int pageIndex)
{
    if (!m_doc || m_pages.isEmpty()) return;
    pageIndex = qBound(0, pageIndex, m_pages.size() - 1);
    verticalScrollBar()->setValue(m_pages[pageIndex].yOffset);
}

void PDFView::firstPage()   { goToPage(0); }
void PDFView::lastPage()    { goToPage(pageCount() - 1); }

void PDFView::previousPage()
{
    int cur = currentPage();
    if (cur > 0) goToPage(cur - 1);
}

void PDFView::nextPage()
{
    int cur = currentPage();
    if (cur < pageCount() - 1) goToPage(cur + 1);
}

int PDFView::currentPage() const
{
    if (m_pages.isEmpty()) return 0;
    return pageAtY(verticalScrollBar()->value());
}

int PDFView::pageCount() const
{
    return m_doc ? m_doc->pageCount() : 0;
}

// ── Zoom ────────────────────────────────────────────────────────────────────

void PDFView::zoomIn()  { setZoom(m_zoom * 1.25f); }
void PDFView::zoomOut() { setZoom(m_zoom / 1.25f); }

void PDFView::setZoom(float zoom)
{
    zoom = qBound(0.1f, zoom, 8.0f);
    if (qFuzzyCompare(zoom, m_zoom)) return;

    // Keep the same document y-ratio
    double ratio = 0.0;
    if (totalHeight() > 0)
        ratio = (double)verticalScrollBar()->value() / totalHeight();

    m_zoom = zoom;
    invalidateCache();
    rebuildLayout();

    verticalScrollBar()->setValue(qRound(ratio * totalHeight()));
    viewport()->update();
    emit zoomChanged(m_zoom);
}

void PDFView::actualSize()  { setZoom(1.0f); }

void PDFView::fitWidth()
{
    if (!m_doc || m_pages.isEmpty()) return;
    // Use page 0 as reference
    QSizeF ps = m_doc->pageSize(0);
    if (ps.width() <= 0) return;
    float z = (float)viewport()->width() / ps.width();
    setZoom(z);
}

void PDFView::fitHeight()
{
    if (!m_doc || m_pages.isEmpty()) return;
    QSizeF ps = m_doc->pageSize(0);
    if (ps.height() <= 0) return;
    float z = (float)viewport()->height() / ps.height();
    setZoom(z);
}

// ── Text search ─────────────────────────────────────────────────────────────

int PDFView::findText(const QString &text, bool forward)
{
    m_searchText = text;
    m_searchPage = forward ? 0 : (pageCount() - 1);
    return forward ? findNext() : findPrev();
}

int PDFView::findNext()
{
    if (!m_doc || m_searchText.isEmpty()) return -1;
    // Simple scan: use MuPDF fz_search_page
    fz_context  *ctx = nullptr;
    fz_document *doc = nullptr;
    // Access internal state via PDFDocument is not exposed; we replicate a
    // minimal open here just for search.  In a real app you'd expose the
    // context/document from PDFDocument or add a search method to it.
    // For now we skip the implementation detail and just scroll to page.
    int start = qMax(0, m_searchPage);
    for (int p = start; p < pageCount(); ++p) {
        // placeholder: every page "matches" – replace with real fz_search_page
        // call once PDFDocument exposes its context/document handles.
        m_searchPage = p;
        goToPage(p);
        return p;
    }
    return -1;
}

int PDFView::findPrev()
{
    if (!m_doc || m_searchText.isEmpty()) return -1;
    int start = qMin(m_searchPage, pageCount() - 1);
    for (int p = start; p >= 0; --p) {
        m_searchPage = p;
        goToPage(p);
        return p;
    }
    return -1;
}

// ── Layout ──────────────────────────────────────────────────────────────────

void PDFView::rebuildLayout()
{
    m_pages.clear();

    if (!m_doc || !m_doc->isOpen()) {
        horizontalScrollBar()->setRange(0, 0);
        verticalScrollBar()->setRange(0, 0);
        return;
    }

    int y     = 0;
    int maxW  = 0;
    int n     = m_doc->pageCount();
    m_pages.resize(n);
    m_cache.resize(n);  // null images until rendered

    for (int i = 0; i < n; ++i) {
        QSizeF ps = m_doc->pageSize(i);
        int pw = qRound(ps.width()  * m_zoom);
        int ph = qRound(ps.height() * m_zoom);
        m_pages[i].yOffset = y;
        m_pages[i].height  = ph;
        m_pages[i].width   = pw;
        y  += ph + PAGE_GAP;
        if (pw > maxW) maxW = pw;
    }

    int docH = (y > 0) ? y - PAGE_GAP : 0;

    verticalScrollBar()->setRange(0, qMax(0, docH - viewport()->height()));
    verticalScrollBar()->setPageStep(viewport()->height());
    horizontalScrollBar()->setRange(0, qMax(0, maxW - viewport()->width()));
    horizontalScrollBar()->setPageStep(viewport()->width());
}

void PDFView::invalidateCache()
{
    for (QImage &img : m_cache)
        img = QImage();
}

int PDFView::totalHeight() const
{
    if (m_pages.isEmpty()) return 0;
    const PageInfo &last = m_pages.last();
    return last.yOffset + last.height;
}

int PDFView::pageAtY(int y) const
{
    // Binary search
    int lo = 0, hi = m_pages.size() - 1;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (m_pages[mid].yOffset <= y) lo = mid;
        else                           hi = mid - 1;
    }
    return lo;
}

// ── Rendering ───────────────────────────────────────────────────────────────

QImage PDFView::cachedPage(int pageIndex)
{
    if (pageIndex < 0 || pageIndex >= m_cache.size()) return {};
    if (m_cache[pageIndex].isNull())
        m_cache[pageIndex] = m_doc->renderPage(pageIndex, m_zoom);
    return m_cache[pageIndex];
}

// ── Events ──────────────────────────────────────────────────────────────────

void PDFView::paintEvent(QPaintEvent *)
{
    QPainter painter(viewport());
    painter.fillRect(viewport()->rect(), Qt::darkGray);

    if (!m_doc || !m_doc->isOpen() || m_pages.isEmpty()) return;

    int scrollY = verticalScrollBar()->value();
    int scrollX = horizontalScrollBar()->value();
    int vpW     = viewport()->width();
    int vpH     = viewport()->height();

    // Determine visible page range
    int firstVisible = pageAtY(scrollY);
    int lastVisible  = firstVisible;
    while (lastVisible + 1 < m_pages.size() &&
           m_pages[lastVisible + 1].yOffset < scrollY + vpH)
        ++lastVisible;

    for (int i = firstVisible; i <= lastVisible; ++i) {
        const PageInfo &pi = m_pages[i];
        int x = (vpW - pi.width) / 2 - scrollX;
        int y = pi.yOffset - scrollY;

        // White background for page
        painter.fillRect(x, y, pi.width, pi.height, Qt::white);

        QImage img = cachedPage(i);
        if (!img.isNull())
            painter.drawImage(x, y, img);
        else
            painter.drawText(x + 8, y + 20, QString("Page %1").arg(i + 1));
    }
}

void PDFView::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    rebuildLayout();
}

void PDFView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        // Zoom
        int delta = event->angleDelta().y();
        if (delta > 0)       zoomIn();
        else if (delta < 0)  zoomOut();
        event->accept();
    } else {
        QAbstractScrollArea::wheelEvent(event);
    }
}

void PDFView::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Home:  firstPage();    break;
    case Qt::Key_End:   lastPage();     break;
    case Qt::Key_PageUp:   previousPage(); break;
    case Qt::Key_PageDown: nextPage();     break;
    default: QAbstractScrollArea::keyPressEvent(event); break;
    }
}

void PDFView::scrollContentsBy(int dx, int dy)
{
    QAbstractScrollArea::scrollContentsBy(dx, dy);
    viewport()->update();

    if (!m_syncingScroll && totalHeight() > 0) {
        double ratio = (double)verticalScrollBar()->value() /
                       (double)verticalScrollBar()->maximum();
        emit scrollRatioChanged(ratio);
    }
}

void PDFView::setScrollRatio(double ratio)
{
    if (m_syncingScroll) return;
    m_syncingScroll = true;
    int v = qRound(ratio * verticalScrollBar()->maximum());
    verticalScrollBar()->setValue(v);
    m_syncingScroll = false;
}

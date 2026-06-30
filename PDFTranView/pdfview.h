#pragma once

#include <QAbstractScrollArea>
#include <QImage>
#include <QVector>

class PDFDocument;

/*
 * PDFView – a scrollable, zoomable PDF page viewer.
 *
 * - Pages are laid out vertically with PAGE_GAP pixels between them.
 * - Only visible pages are rendered (lazy / on-demand via a cache).
 * - Emits scrollChanged(int value, int maximum) so a sibling PDFView can
 *   follow in sync.
 */
class PDFView : public QAbstractScrollArea
{
    Q_OBJECT

public:
    explicit PDFView(QWidget *parent = nullptr);

    void setDocument(PDFDocument *doc);
    PDFDocument *document() const { return m_doc; }

    // Navigation
    void goToPage(int pageIndex);
    void firstPage();
    void lastPage();
    void previousPage();
    void nextPage();
    int  currentPage() const;
    int  pageCount()   const;

    // Zoom
    void  zoomIn();
    void  zoomOut();
    void  setZoom(float zoom);
    float zoom() const { return m_zoom; }
    void  fitWidth();
    void  fitHeight();
    void  actualSize();   // 1.0

    // Text search (basic: returns page index of first match, -1 if none)
    int  findText(const QString &text, bool forward = true);
    int  findNext();
    int  findPrev();

signals:
    // Fires whenever the vertical scroll or zoom changes so a paired view
    // can stay in sync.  ratio is in [0, 1].
    void scrollRatioChanged(double ratio);
    void zoomChanged(float zoom);

public slots:
    // Called by the paired view to sync without re-emitting.
    void setScrollRatio(double ratio);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    struct PageInfo {
        int   yOffset = 0;   // top of page in document coords
        int   height  = 0;   // page height at zoom 1.0 (points)
        int   width   = 0;
    };

    void   rebuildLayout();
    void   invalidateCache();
    QImage cachedPage(int pageIndex);

    // Compute which page is at the top of the viewport
    int  pageAtY(int y) const;
    int  totalHeight() const;

    PDFDocument           *m_doc   = nullptr;
    float                  m_zoom  = 1.0f;
    QVector<PageInfo>      m_pages;
    QVector<QImage>        m_cache;    // indexed by page

    QString  m_searchText;
    int      m_searchPage = -1;

    bool     m_syncingScroll = false;  // re-entrancy guard

    static constexpr int PAGE_GAP = 8;
};

#include "mainwindow.h"
#include "pdfview.h"
#include "pdfdocument.h"

#include <QApplication>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QSplitter>
#include <QLabel>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QDir>
#include <QComboBox>
#include <QAction>
#include <QMenu>
#include <QToolBar>

// ── ctor / dtor ─────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_srcDoc(new PDFDocument)
    , m_trnDoc(new PDFDocument)
    , m_engine(new QProcess(this))
{
    setWindowTitle(tr("PDF Translator"));
    setAcceptDrops(true);
    resize(1280, 800);

    createCentralWidget();
    createActions();
    createMenus();
    createToolBar();
    createStatusBar();

    connect(m_engine, SIGNAL(finished(int,QProcess::ExitStatus)),
            this,     SLOT(onEngineFinished(int)));
    connect(m_engine, SIGNAL(readyReadStandardOutput()),
            this,     SLOT(onEngineOutput()));
    connect(m_engine, SIGNAL(readyReadStandardError()),
            this,     SLOT(onEngineOutput()));

    // Restore recent files
    QSettings s("PDFTranslator", "PDFTranView");
    m_recentFiles = s.value("recentFiles").toStringList();
    rebuildRecentMenu();

    m_srcLang = s.value("srcLang", "Chinese").toString();
    m_tgtLang = s.value("tgtLang", "English").toString();
}

MainWindow::~MainWindow()
{
    if (m_engine->state() != QProcess::NotRunning)
        m_engine->kill();
    delete m_srcDoc;
    delete m_trnDoc;
}

// ── Central widget ───────────────────────────────────────────────────────────

void MainWindow::createCentralWidget()
{
    m_splitter  = new QSplitter(Qt::Horizontal, this);
    m_leftView  = new PDFView(m_splitter);
    m_rightView = new PDFView(m_splitter);
    m_splitter->addWidget(m_leftView);
    m_splitter->addWidget(m_rightView);
    m_splitter->setSizes({640, 640});
    setCentralWidget(m_splitter);

    m_rightView->setVisible(false);

    // Zoom sync: when user changes zoom in left, mirror to right and vice versa
    connect(m_leftView, &PDFView::zoomChanged, m_rightView, &PDFView::setZoom);
    connect(m_rightView,&PDFView::zoomChanged, m_leftView,  &PDFView::setZoom);

    // Scroll sync (enabled by default)
    syncScrollSetup(true);

    connect(m_leftView, &PDFView::zoomChanged,
            this, &MainWindow::onZoomChanged);
}

// ── Actions & Menus ─────────────────────────────────────────────────────────

void MainWindow::createActions()
{
    // pre-create actions used elsewhere
    m_actClose     = new QAction(tr("&Close"),           this);
    m_actStartTran = new QAction(tr("&Start (Ctrl+T)"),  this);
    m_actStopTran  = new QAction(tr("S&top"),            this);
    m_actFindNext  = new QAction(tr("Find &Next (F3)"),  this);
    m_actFindPrev  = new QAction(tr("Find &Previous"),   this);

    m_actClose->setEnabled(false);
    m_actStartTran->setEnabled(false);
    m_actStopTran->setEnabled(false);
    m_actFindNext->setEnabled(false);
    m_actFindPrev->setEnabled(false);
}

void MainWindow::createMenus()
{
    // ── File ────────────────────────────────────────────────────────────────
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));

    QAction *actOpen = fileMenu->addAction(tr("&Open…\tCtrl+O"), this, SLOT(onOpen()));
    actOpen->setShortcut(QKeySequence::Open);

    m_actClose->setShortcut(QKeySequence::Close);
    connect(m_actClose, SIGNAL(triggered()), this, SLOT(onClose()));
    fileMenu->addAction(m_actClose);

    fileMenu->addSeparator();
    m_recentMenu = fileMenu->addMenu(tr("Recent Files"));

    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), qApp, SLOT(quit()), QKeySequence::Quit);

    // ── View ────────────────────────────────────────────────────────────────
    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));

    QMenu *gotoMenu = viewMenu->addMenu(tr("&Go to"));
    gotoMenu->addAction(tr("First Page\tHome"),  this, SLOT(onGotoFirstPage()),  Qt::Key_Home);
    gotoMenu->addAction(tr("Last Page\tEnd"),    this, SLOT(onGotoLastPage()),   Qt::Key_End);
    gotoMenu->addAction(tr("Previous Page\tPgUp"), this, SLOT(onGotoPrevPage()), Qt::Key_PageUp);
    gotoMenu->addAction(tr("Next Page\tPgDn"),   this, SLOT(onGotoNextPage()),  Qt::Key_PageDown);
    gotoMenu->addAction(tr("Page Number…\tCtrl+G"), this, SLOT(onGotoPageNumber()),
                        QKeySequence("Ctrl+G"));

    viewMenu->addSeparator();
    QMenu *sizeMenu = viewMenu->addMenu(tr("&Zoom"));
    sizeMenu->addAction(tr("Zoom In\tCtrl+="),    this, SLOT(onZoomIn()),      QKeySequence::ZoomIn);
    sizeMenu->addAction(tr("Zoom Out\tCtrl+-"),   this, SLOT(onZoomOut()),     QKeySequence::ZoomOut);
    sizeMenu->addSeparator();
    sizeMenu->addAction(tr("Actual Size\tCtrl+0"),  this, SLOT(onActualWidth()),  QKeySequence("Ctrl+0"));
    sizeMenu->addAction(tr("Fit Width\tCtrl+1"),    this, SLOT(onScreenWidth()),  QKeySequence("Ctrl+1"));
    sizeMenu->addAction(tr("Fit Height\tCtrl+2"),   this, SLOT(onScreenHeight()), QKeySequence("Ctrl+2"));

    viewMenu->addSeparator();
    QAction *actSync = viewMenu->addAction(tr("Sync Scroll"));
    actSync->setCheckable(true);
    actSync->setChecked(true);
    connect(actSync, SIGNAL(toggled(bool)), this, SLOT(onToggleSyncScroll(bool)));

    // ── Tools ────────────────────────────────────────────────────────────────
    QMenu *toolsMenu = menuBar()->addMenu(tr("&Tools"));

    QAction *actFind = toolsMenu->addAction(tr("&Find…\tCtrl+F"), this, SLOT(onFind()),
                                            QKeySequence::Find);

    m_actFindNext->setShortcut(Qt::Key_F3);
    m_actFindPrev->setShortcut(QKeySequence("Shift+F3"));
    connect(m_actFindNext, SIGNAL(triggered()), this, SLOT(onFindNext()));
    connect(m_actFindPrev, SIGNAL(triggered()), this, SLOT(onFindPrev()));
    toolsMenu->addAction(m_actFindNext);
    toolsMenu->addAction(m_actFindPrev);

    // ── Translation ──────────────────────────────────────────────────────────
    QMenu *tranMenu = menuBar()->addMenu(tr("&Translation"));

    m_actStartTran->setShortcut(QKeySequence("Ctrl+T"));
    m_actStopTran->setShortcut(QKeySequence("Ctrl+Shift+B"));
    connect(m_actStartTran, SIGNAL(triggered()), this, SLOT(onStartTranslation()));
    connect(m_actStopTran,  SIGNAL(triggered()), this, SLOT(onStopTranslation()));
    tranMenu->addAction(m_actStartTran);
    tranMenu->addAction(m_actStopTran);

    // ── Help ─────────────────────────────────────────────────────────────────
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About…"), this, SLOT(onAbout()));

    Q_UNUSED(actFind)
}

void MainWindow::createToolBar()
{
    QToolBar *tb = addToolBar(tr("Main"));
    tb->setMovable(false);

    tb->addAction(tr("Open"), this, SLOT(onOpen()));
    tb->addSeparator();

    tb->addAction(tr("|<"),  this, SLOT(onGotoFirstPage()));
    tb->addAction(tr("<"),   this, SLOT(onGotoPrevPage()));
    tb->addAction(tr(">"),   this, SLOT(onGotoNextPage()));
    tb->addAction(tr(">|"),  this, SLOT(onGotoLastPage()));

    tb->addSeparator();
    tb->addAction(tr("Z+"), this, SLOT(onZoomIn()));
    tb->addAction(tr("Z-"), this, SLOT(onZoomOut()));

    tb->addSeparator();

    // Language selectors
    QLabel *srcLbl = new QLabel(tr(" Src:"), tb);
    tb->addWidget(srcLbl);
    QComboBox *srcBox = new QComboBox(tb);
    srcBox->addItems({"Chinese","English","Japanese","Korean","French",
                      "German","Spanish","Arabic","Russian","Auto"});
    srcBox->setCurrentText(m_srcLang);
    tb->addWidget(srcBox);
    connect(srcBox, &QComboBox::currentTextChanged,
            [this](const QString &t){ m_srcLang = t; });

    QLabel *tgtLbl = new QLabel(tr("  Dst:"), tb);
    tb->addWidget(tgtLbl);
    QComboBox *tgtBox = new QComboBox(tb);
    tgtBox->addItems({"English","Chinese","Japanese","Korean","French",
                      "German","Spanish","Arabic","Russian"});
    tgtBox->setCurrentText(m_tgtLang);
    tb->addWidget(tgtBox);
    connect(tgtBox, &QComboBox::currentTextChanged,
            [this](const QString &t){ m_tgtLang = t; });

    tb->addSeparator();
    tb->addAction(tr("Translate"), this, SLOT(onStartTranslation()));
}

void MainWindow::createStatusBar()
{
    m_statusPage = new QLabel(tr("No document"), statusBar());
    m_statusZoom = new QLabel("100%", statusBar());
    m_statusMsg  = new QLabel("", statusBar());

    statusBar()->addWidget(m_statusPage, 1);
    statusBar()->addPermanentWidget(m_statusZoom);
    statusBar()->addPermanentWidget(m_statusMsg);
}

// ── File operations ──────────────────────────────────────────────────────────

void MainWindow::onOpen()
{
    QString path = QFileDialog::getOpenFileName(
        this, tr("Open PDF"), QString(),
        tr("PDF Files (*.pdf);;All Files (*)"));
    if (!path.isEmpty())
        openFile(path);
}

void MainWindow::onClose()
{
    m_leftView->setDocument(nullptr);
    m_rightView->setDocument(nullptr);
    m_rightView->setVisible(false);

    if (m_srcDoc->isOpen()) m_srcDoc->close();
    if (m_trnDoc->isOpen()) m_trnDoc->close();

    m_srcPath.clear();
    setWindowTitle(tr("PDF Translator"));
    m_actClose->setEnabled(false);
    m_actStartTran->setEnabled(false);
    updateStatusBar();
}

void MainWindow::onOpenRecent()
{
    QAction *act = qobject_cast<QAction*>(sender());
    if (act) openFile(act->data().toString());
}

void MainWindow::openFile(const QString &path)
{
    if (!m_srcDoc->open(path)) {
        QMessageBox::warning(this, tr("Error"),
            tr("Cannot open:\n%1").arg(path));
        return;
    }

    m_srcPath = path;
    m_leftView->setDocument(m_srcDoc);
    m_rightView->setVisible(false);

    setWindowTitle(tr("PDF Translator – %1").arg(QFileInfo(path).fileName()));
    m_actClose->setEnabled(true);
    m_actStartTran->setEnabled(true);
    m_actFindNext->setEnabled(true);
    m_actFindPrev->setEnabled(true);

    updateRecentFiles(path);
    updateStatusBar();

    // If a translated version exists, open it automatically
    QString tranPath = translateOutputPath(path);
    if (QFileInfo::exists(tranPath)) {
        if (m_trnDoc->open(tranPath)) {
            m_rightView->setDocument(m_trnDoc);
            m_rightView->setVisible(true);
        }
    }
}

void MainWindow::updateRecentFiles(const QString &path)
{
    m_recentFiles.removeAll(path);
    m_recentFiles.prepend(path);
    while (m_recentFiles.size() > MAX_RECENT)
        m_recentFiles.removeLast();

    QSettings s("PDFTranslator", "PDFTranView");
    s.setValue("recentFiles", m_recentFiles);
    rebuildRecentMenu();
}

void MainWindow::rebuildRecentMenu()
{
    if (!m_recentMenu) return;
    m_recentMenu->clear();
    for (const QString &f : m_recentFiles) {
        QAction *a = m_recentMenu->addAction(QFileInfo(f).fileName());
        a->setData(f);
        a->setToolTip(f);
        connect(a, SIGNAL(triggered()), this, SLOT(onOpenRecent()));
    }
    m_recentMenu->setEnabled(!m_recentFiles.isEmpty());
}

QString MainWindow::translateOutputPath(const QString &srcPath) const
{
    QFileInfo fi(srcPath);
    return fi.dir().filePath(fi.completeBaseName() + "_tran.pdf");
}

// ── View slots ───────────────────────────────────────────────────────────────

void MainWindow::onGotoFirstPage()   { m_leftView->firstPage(); }
void MainWindow::onGotoLastPage()    { m_leftView->lastPage();  }
void MainWindow::onGotoPrevPage()    { m_leftView->previousPage(); }
void MainWindow::onGotoNextPage()    { m_leftView->nextPage();  }

void MainWindow::onGotoPageNumber()
{
    if (!m_srcDoc->isOpen()) return;
    bool ok;
    int pg = QInputDialog::getInt(this, tr("Go to Page"),
                 tr("Page number (1–%1):").arg(m_srcDoc->pageCount()),
                 m_leftView->currentPage() + 1,
                 1, m_srcDoc->pageCount(), 1, &ok);
    if (ok) m_leftView->goToPage(pg - 1);
}

void MainWindow::onZoomIn()      { m_leftView->zoomIn(); }
void MainWindow::onZoomOut()     { m_leftView->zoomOut(); }
void MainWindow::onActualWidth() { m_leftView->actualSize(); }
void MainWindow::onScreenWidth() { m_leftView->fitWidth(); }
void MainWindow::onScreenHeight(){ m_leftView->fitHeight(); }

void MainWindow::onToggleSideBySide(bool checked) { Q_UNUSED(checked) }

void MainWindow::onToggleSyncScroll(bool checked)
{
    m_syncScroll = checked;
    syncScrollSetup(checked);
}

void MainWindow::syncScrollSetup(bool enable)
{
    if (enable) {
        connect(m_leftView,  &PDFView::scrollRatioChanged,
                m_rightView, &PDFView::setScrollRatio, Qt::UniqueConnection);
        connect(m_rightView, &PDFView::scrollRatioChanged,
                m_leftView,  &PDFView::setScrollRatio, Qt::UniqueConnection);
    } else {
        disconnect(m_leftView,  &PDFView::scrollRatioChanged,
                   m_rightView, &PDFView::setScrollRatio);
        disconnect(m_rightView, &PDFView::scrollRatioChanged,
                   m_leftView,  &PDFView::setScrollRatio);
    }
}

// ── Tools ────────────────────────────────────────────────────────────────────

void MainWindow::onFind()
{
    bool ok;
    QString text = QInputDialog::getText(this, tr("Find"),
                       tr("Search text:"), QLineEdit::Normal, QString(), &ok);
    if (ok && !text.isEmpty())
        m_leftView->findText(text, true);
}

void MainWindow::onFindNext() { m_leftView->findNext(); }
void MainWindow::onFindPrev() { m_leftView->findPrev(); }

// ── Translation ──────────────────────────────────────────────────────────────

void MainWindow::onStartTranslation()
{
    if (!m_srcDoc->isOpen()) return;
    if (m_engine->state() != QProcess::NotRunning) return;

    m_statusMsg->setText(tr("Translating…"));
    m_actStartTran->setEnabled(false);
    m_actStopTran->setEnabled(true);

    // Locate PDFTranEngine next to our own executable
    QString enginePath = QApplication::applicationDirPath() + "/PDFTranEngine";
#ifdef Q_OS_WIN
    enginePath += ".exe";
#endif

    QStringList args;
    args << m_srcPath
         << m_srcLang
         << m_tgtLang;

    m_engine->start(enginePath, args);
}

void MainWindow::onStopTranslation()
{
    if (m_engine->state() != QProcess::NotRunning) {
        m_engine->kill();
        m_statusMsg->setText(tr("Translation stopped."));
    }
    m_actStartTran->setEnabled(m_srcDoc->isOpen());
    m_actStopTran->setEnabled(false);
}

void MainWindow::onEngineFinished(int exitCode)
{
    m_actStartTran->setEnabled(m_srcDoc->isOpen());
    m_actStopTran->setEnabled(false);

    if (exitCode == 0) {
        m_statusMsg->setText(tr("Translation complete."));
        QString tranPath = translateOutputPath(m_srcPath);
        if (m_trnDoc->isOpen()) m_trnDoc->close();
        if (m_trnDoc->open(tranPath)) {
            m_rightView->setDocument(m_trnDoc);
            m_rightView->setVisible(true);
            // align zoom and scroll
            m_rightView->setZoom(m_leftView->zoom());
        } else {
            QMessageBox::warning(this, tr("Warning"),
                tr("Translation finished but output file not found:\n%1")
                .arg(tranPath));
        }
    } else {
        m_statusMsg->setText(tr("Translation failed (exit %1).").arg(exitCode));
        QByteArray err = m_engine->readAllStandardError();
        if (!err.isEmpty())
            QMessageBox::warning(this, tr("Translation Error"), QString::fromLocal8Bit(err));
    }
}

void MainWindow::onEngineOutput()
{
    QByteArray out = m_engine->readAllStandardOutput();
    QByteArray err = m_engine->readAllStandardError();
    QString combined = QString::fromLocal8Bit(out + err).trimmed();
    if (!combined.isEmpty())
        m_statusMsg->setText(combined.left(120));
}

// ── Help ─────────────────────────────────────────────────────────────────────

void MainWindow::onAbout()
{
    QMessageBox::about(this, tr("About PDF Translator"),
        tr("<b>PDF Translator v1.0</b><br/><br/>"
           "A side-by-side PDF viewer with offline translation support.<br/><br/>"
           "PDF rendering by MuPDF (AGPL).<br/>"
           "Translation engine: PDFTranEngine."));
}

// ── Status bar ────────────────────────────────────────────────────────────────

void MainWindow::onZoomChanged(float zoom)
{
    m_statusZoom->setText(QString("%1%").arg(qRound(zoom * 100)));
}

void MainWindow::updateStatusBar()
{
    if (m_srcDoc->isOpen())
        m_statusPage->setText(
            tr("Page %1 / %2")
            .arg(m_leftView->currentPage() + 1)
            .arg(m_srcDoc->pageCount()));
    else
        m_statusPage->setText(tr("No document"));
}

// ── Window events ─────────────────────────────────────────────────────────────

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_engine->state() != QProcess::NotRunning) {
        m_engine->kill();
        m_engine->waitForFinished(2000);
    }
    QSettings s("PDFTranslator", "PDFTranView");
    s.setValue("recentFiles", m_recentFiles);
    s.setValue("srcLang", m_srcLang);
    s.setValue("tgtLang", m_tgtLang);
    QMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    for (const QUrl &url : event->mimeData()->urls()) {
        QString path = url.toLocalFile();
        if (path.toLower().endsWith(".pdf")) {
            openFile(path);
            break;
        }
    }
}

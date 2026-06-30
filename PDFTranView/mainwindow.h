#pragma once

#include <QMainWindow>
#include <QStringList>

class PDFDocument;
class PDFView;
class QSplitter;
class QLabel;
class QProcess;
class QAction;
class QMenu;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

public slots:
    void openFile(const QString &path);   // also callable via invokeMethod

private slots:
    // File
    void onOpen();
    void onClose();
    void onOpenRecent();

    // View – Go-to
    void onGotoFirstPage();
    void onGotoLastPage();
    void onGotoPrevPage();
    void onGotoNextPage();
    void onGotoPageNumber();

    // View – Zoom
    void onZoomIn();
    void onZoomOut();
    void onActualWidth();
    void onScreenWidth();
    void onScreenHeight();

    // View – mode
    void onToggleSideBySide(bool checked);
    void onToggleSyncScroll(bool checked);

    // Tools
    void onFind();
    void onFindNext();
    void onFindPrev();

    // Translation
    void onStartTranslation();
    void onStopTranslation();
    void onEngineFinished(int exitCode);
    void onEngineOutput();

    // Help
    void onAbout();

    // Zoom label update
    void onZoomChanged(float zoom);

    // Page status update
    void updateStatusBar();

private:
    void createActions();
    void createMenus();
    void createToolBar();
    void createStatusBar();
    void createCentralWidget();

    void updateRecentFiles(const QString &path);
    void rebuildRecentMenu();
    void syncScrollSetup(bool enable);

    QString translateOutputPath(const QString &srcPath) const;

    // Widgets
    QSplitter   *m_splitter       = nullptr;
    PDFView     *m_leftView       = nullptr;   // source PDF
    PDFView     *m_rightView      = nullptr;   // translated PDF
    QLabel      *m_statusPage     = nullptr;
    QLabel      *m_statusZoom     = nullptr;
    QLabel      *m_statusMsg      = nullptr;

    // Documents
    PDFDocument *m_srcDoc         = nullptr;
    PDFDocument *m_trnDoc         = nullptr;

    // Translation process
    QProcess    *m_engine         = nullptr;
    QString      m_srcPath;

    // Recent files
    static constexpr int MAX_RECENT = 5;
    QStringList  m_recentFiles;
    QMenu       *m_recentMenu     = nullptr;

    // Settings
    bool         m_syncScroll     = true;

    // Language selection
    QString      m_srcLang        = "Chinese";
    QString      m_tgtLang        = "English";

    // Actions (kept for enable/disable management)
    QAction *m_actClose        = nullptr;
    QAction *m_actStartTran    = nullptr;
    QAction *m_actStopTran     = nullptr;
    QAction *m_actFindNext     = nullptr;
    QAction *m_actFindPrev     = nullptr;
};

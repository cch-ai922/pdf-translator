#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("PDFTranView");
    app.setOrganizationName("PDFTranslator");

    MainWindow w;
    w.show();

    // Support opening a file from command line: PDFTranView myfile.pdf
    if (argc > 1) {
        // mainwindow slot is private, use invokeMethod or call directly via friend.
        // Simpler: post an event processed after the event loop starts.
        QMetaObject::invokeMethod(&w, "openFile",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, QString::fromLocal8Bit(argv[1])));
    }

    return app.exec();
}

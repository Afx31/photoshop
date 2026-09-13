#include "window.h"

#include <QApplication>
#include <QFileInfo>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Photo"));
    app.setApplicationDisplayName(QStringLiteral("Photo"));
    app.setOrganizationName(QStringLiteral("photo"));
    app.setDesktopFileName(QStringLiteral("photo"));
    PhotoWindow win;
    win.show();
    if (argc > 1) {
        const QString path = QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath();
        if (!path.isEmpty())
            win.loadPath(path);
    }
    return app.exec();
}

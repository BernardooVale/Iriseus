#include <QApplication>
#include <QMessageBox>
#include "app/Application.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString& msg) {
        fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
        fflush(stderr);
    });

    Application iriseus;
    iriseus.init();
    return app.exec();
}
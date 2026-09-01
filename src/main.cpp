#include <QApplication>
#include <QMessageBox>
#include "app/Application.h"

int main(int argc, char* argv[])
{
    QApplication qt(argc, argv);
    qt.setQuitOnLastWindowClosed(false);

    Application app;

    if (!app.init()) {
        return 1;
    }

    return qt.exec();
}
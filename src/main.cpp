#include <QApplication>
#include "app/Application.h"

int main(int argc, char* argv[])
{
    // Systray exige QApplication, não QCoreApplication
    QApplication qt(argc, argv);
    qt.setQuitOnLastWindowClosed(false); // app vive no systray, não em janelas

    Application app;
    if (!app.init()) {
        return 1;
    }

    return qt.exec();
}
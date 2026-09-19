#include <QApplication>
#include <QMessageBox>
#include "app/Application.h"
#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char* argv[])
{
#ifdef _WIN32
    // Garante instância única no Windows — evita duas instâncias concorrentes com PINs diferentes
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Local\\IriseusSingleInstanceMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        return 0; // Já existe uma instância rodando
    }
#endif

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString& msg) {
        fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
        fflush(stderr);
    });

    Application iriseus;
    iriseus.init();
    int ret = app.exec();

#ifdef _WIN32
    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }
#endif

    return ret;
}
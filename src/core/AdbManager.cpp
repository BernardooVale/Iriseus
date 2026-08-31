#include "AdbManager.h"
#include <windows.h>
#include <shlobj.h>
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QString>
#include <QDebug>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>

// Binários ADB devem estar em resources/adb/
// adb.exe, AdbWinApi.dll, AdbWinUsbApi.dll
// Adicionados ao resources.qrc como arquivos binários

static constexpr const char* kAdbServerPort = "5038";

std::wstring AdbManager::adbDir()
{
    wchar_t path[MAX_PATH];
    SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, path);
    return std::wstring(path) + L"\\DevLink\\adb";
}

std::wstring AdbManager::adbExe()
{
    return adbDir() + L"\\adb.exe";
}

AdbManager::AdbManager(uint16_t wsPort, uint16_t streamPort)
    : m_wsPort(wsPort)
    , m_streamPort(streamPort)
{}

AdbManager::~AdbManager()
{
    stop();
}

bool AdbManager::init()
{
    if (!extractAdb()) return false;
    if (!startAdbServer()) return false;
    return true;
}

void AdbManager::start()
{
    m_running = true;
    m_thread  = std::thread(&AdbManager::monitorLoop, this);
}

void AdbManager::stop()
{
    if (!m_running.exchange(false)) return;
    if (m_thread.joinable()) m_thread.join();
    // Não mata o servidor ADB ao parar — evita disrução se usuário reiniciar app
}

bool AdbManager::extractAdb()
{
    auto dir = QString::fromStdWString(adbDir());
    QDir().mkpath(dir);

    // Lista de binários bundlados no resources.qrc
    static const char* files[] = {
        ":/adb/adb.exe",
        ":/adb/AdbWinApi.dll",
        ":/adb/AdbWinUsbApi.dll"
    };

    for (auto* resPath : files) {
        QString fileName = QString(resPath).section('/', -1);
        QString destPath = dir + "/" + fileName;

        if (QFile::exists(destPath)) continue; // já extraído

        if (!QFile::copy(resPath, destPath)) {
            qWarning() << "AdbManager: falha ao extrair" << resPath;
            return false;
        }
        // adb.exe precisa ser executável
        QFile::setPermissions(destPath, QFileDevice::ReadOwner |
                                        QFileDevice::WriteOwner |
                                        QFileDevice::ExeOwner);
    }
    return true;
}

bool AdbManager::startAdbServer()
{
    auto result = runAdb({"start-server"});
    return true; // start-server sempre retorna 0 mesmo se já rodando
}

bool AdbManager::setupReverse(const std::string& serial)
{
    // adb -s <serial> reverse tcp:<wsPort> tcp:<wsPort>
    auto ws  = std::to_string(m_wsPort);
    auto str = std::to_string(m_streamPort);

    runAdb({"-s", serial, "reverse", "tcp:" + ws,  "tcp:" + ws});
    runAdb({"-s", serial, "reverse", "tcp:" + str, "tcp:" + str});

    qDebug() << "AdbManager: reverse configurado para" << serial.c_str();
    return true;
}

void AdbManager::removeReverse(const std::string& serial)
{
    runAdb({"-s", serial, "reverse", "--remove-all"});
}

std::string AdbManager::runAdb(const std::vector<std::string>& args)
{
    // Monta linha de comando
    std::wstring cmd = L"\"" + adbExe() + L"\"";
    for (auto& a : args) {
        cmd += L" ";
        cmd += std::wstring(a.begin(), a.end());
    }

    // Variável de ambiente isolada para porta customizada
    SetEnvironmentVariableA("ANDROID_ADB_SERVER_PORT", kAdbServerPort);

    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE hRead, hWrite;
    CreatePipe(&hRead, &hWrite, &sa, 0);
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb         = sizeof(si);
    si.dwFlags    = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWrite;
    si.hStdError  = hWrite;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    std::wstring cmdMut = cmd;
    bool ok = CreateProcessW(nullptr, cmdMut.data(), nullptr, nullptr,
                             TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(hWrite);

    std::string output;
    if (ok) {
        char buf[256];
        DWORD read;
        while (ReadFile(hRead, buf, sizeof(buf) - 1, &read, nullptr) && read > 0) {
            buf[read] = '\0';
            output += buf;
        }
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    CloseHandle(hRead);
    return output;
}

std::vector<AdbDevice> AdbManager::listDevices()
{
    auto output = runAdb({"devices"});
    std::vector<AdbDevice> devices;

    std::istringstream ss(output);
    std::string line;
    bool first = true;
    while (std::getline(ss, line)) {
        if (first) { first = false; continue; } // pula "List of devices attached"
        if (line.empty() || line == "\r") continue;

        std::istringstream ls(line);
        AdbDevice dev;
        ls >> dev.serial >> dev.state;
        if (!dev.serial.empty())
            devices.push_back(dev);
    }
    return devices;
}

void AdbManager::monitorLoop()
{
    using namespace std::chrono_literals;

    while (m_running) {
        auto devices = listDevices();

        // Detecta novos dispositivos
        for (auto& dev : devices) {
            if (dev.state != "device") continue;
            bool known = std::find(m_activeSerials.begin(),
                                   m_activeSerials.end(),
                                   dev.serial) != m_activeSerials.end();
            if (!known) {
                setupReverse(dev.serial);
                m_activeSerials.push_back(dev.serial);
                if (m_onAttached) m_onAttached(dev);
            }
        }

        // Detecta dispositivos removidos
        for (auto it = m_activeSerials.begin(); it != m_activeSerials.end(); ) {
            bool found = std::any_of(devices.begin(), devices.end(),
                [&](const AdbDevice& d) { return d.serial == *it; });
            if (!found) {
                if (m_onDetached) m_onDetached(*it);
                it = m_activeSerials.erase(it);
            } else {
                ++it;
            }
        }

        std::this_thread::sleep_for(2s); // poll a cada 2s
    }
}
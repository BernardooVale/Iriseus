#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <vector>

// Gerencia ADB bundlado — porta isolada 5038, path absoluto
// Detecta dispositivos Android via USB e configura adb reverse automaticamente

struct AdbDevice {
    std::string serial;
    std::string state; // "device", "offline", "unauthorized"
};

class AdbManager
{
public:
    using OnDeviceAttached = std::function<void(const AdbDevice&)>;
    using OnDeviceDetached = std::function<void(const std::string& serial)>;

    AdbManager(uint16_t wsPort, uint16_t streamPort);
    ~AdbManager();

    bool init();   // extrai adb.exe para %LOCALAPPDATA%, inicia servidor
    void start();  // inicia loop de monitoramento
    void stop();

    void setOnDeviceAttached(OnDeviceAttached cb) { m_onAttached = std::move(cb); }
    void setOnDeviceDetached(OnDeviceDetached cb) { m_onDetached = std::move(cb); }

    static std::wstring adbDir();   // %LOCALAPPDATA%\DevLink\adb
    static std::wstring adbExe();   // %LOCALAPPDATA%\DevLink\adb\adb.exe

private:
    bool extractAdb();              // copia binários do recurso Qt para disco
    bool startAdbServer();
    bool setupReverse(const std::string& serial);
    void removeReverse(const std::string& serial);
    std::string runAdb(const std::vector<std::string>& args);
    std::vector<AdbDevice> listDevices();
    void monitorLoop();

    uint16_t          m_wsPort;
    uint16_t          m_streamPort;
    std::thread       m_thread;
    std::atomic<bool> m_running{false};

    // seriais atualmente com reverse configurado
    std::vector<std::string> m_activeSerials;

    OnDeviceAttached m_onAttached;
    OnDeviceDetached m_onDetached;
};
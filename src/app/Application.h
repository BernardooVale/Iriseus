#pragma once
#include <memory>

class TrayIcon;
class WsServer;
class PairingManager;
class PairingDialog;
class AdbManager;
class StreamReceiver;

namespace Iriseus {
    class FileTransferManager;
}

class Application
{
public:
    Application();
    ~Application();

    bool init();

private:
    void setupServerCallbacks();

    std::unique_ptr<TrayIcon>                    m_tray;
    std::unique_ptr<WsServer>                    m_wsServer;
    std::unique_ptr<PairingManager>              m_pairing;
    PairingDialog*                               m_pairingDialog = nullptr;
    std::unique_ptr<AdbManager>                  m_adb;
    std::unique_ptr<StreamReceiver>              m_stream;
    std::unique_ptr<Iriseus::FileTransferManager> m_fileManager;
};
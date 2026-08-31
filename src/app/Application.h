#pragma once
#include <memory>

class TrayIcon;
class WsServer;
class MdnsService;
class PairingManager;
class PairingDialog;

class Application
{
public:
    Application();
    ~Application();

    bool init();

private:
    void setupServerCallbacks();

    std::unique_ptr<TrayIcon>       m_tray;
    std::unique_ptr<WsServer>       m_wsServer;
    std::unique_ptr<MdnsService>    m_mdns;
    std::unique_ptr<PairingManager> m_pairing;
    PairingDialog*                  m_pairingDialog = nullptr;
};
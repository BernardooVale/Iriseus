#pragma once
#include <memory>

class TrayIcon;
class WsServer;
class MdnsService;

class Application
{
public:
    Application();
    ~Application();

    bool init();

private:
    void setupServerCallbacks();

    std::unique_ptr<TrayIcon> m_tray;
    std::unique_ptr<WsServer>  m_wsServer;
    std::unique_ptr<MdnsService> m_mdns;
};
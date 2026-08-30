#pragma once
#include <memory>

class TrayIcon;

class Application
{
public:
    Application();
    ~Application();

    bool init();

private:
    std::unique_ptr<TrayIcon> m_tray;
};
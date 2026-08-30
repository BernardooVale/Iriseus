#include "Application.h"
#include "ui/TrayIcon.h"

Application::Application() = default;
Application::~Application() = default;

bool Application::init()
{
    m_tray = std::make_unique<TrayIcon>();
    m_tray->show();
    return true;
}
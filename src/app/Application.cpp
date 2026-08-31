#include "Application.h"
#include "ui/TrayIcon.h"
#include "core/WsServer.h"
#include <QDebug>
#include "core/MdnsService.h"

Application::Application() = default;
Application::~Application() = default;

bool Application::init()
{

    m_wsServer = std::make_unique<WsServer>(45678);
    setupServerCallbacks();

    if (!m_wsServer->start()) {
        qWarning() << "DevLink: falha ao iniciar servidor WebSocket na porta 45678";
        return false;
    }

    m_mdns = std::make_unique<MdnsService>("devlink-pc", 45678);
    m_mdns->start();

    m_tray = std::make_unique<TrayIcon>();
    m_tray->show();
    return true;
}

void Application::setupServerCallbacks()
{
    ServerCallbacks cb;

    cb.onDeviceConnected = [this](uint64_t id, const std::string& name) {
        qDebug() << "Dispositivo conectado:" << QString::fromStdString(name) << "id=" << id;
        // TODO: notificar TrayIcon para atualizar status
    };
    cb.onDeviceDisconnected = [this](uint64_t id) {
        qDebug() << "Dispositivo desconectado id=" << id;
    };
    cb.onStartCamera = [this](uint64_t /*id*/) {
        qDebug() << "StartCamera solicitado";
        // TODO: acionar pipeline FFmpeg → Softcam
    };
    cb.onStopCamera = [this](uint64_t /*id*/) {
        qDebug() << "StopCamera solicitado";
    };

    m_wsServer->setCallbacks(std::move(cb));
}
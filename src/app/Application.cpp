#include "Application.h"
#include "ui/TrayIcon.h"
#include "core/WsServer.h"
#include "core/MdnsService.h"
#include "core/PairingManager.h"
#include "ui/PairingDialog.h"
#include <QApplication>
#include <QDebug>
#include "core/AdbManager.h"
#include "core/StreamReceiver.h"

Application::Application() = default;
Application::~Application() = default;

bool Application::init()
{
    m_wsServer = std::make_unique<WsServer>(45678);
    setupServerCallbacks();

    if (!m_wsServer->start()) {
        qWarning() << "Iriseus: falha ao iniciar servidor WebSocket na porta 45678";
        return false;
    }

    m_mdns = std::make_unique<MdnsService>("iriseus-pc", 45678);
    m_mdns->start();

    m_pairing = std::make_unique<PairingManager>(45678);
    m_pairing->setOnPaired([this](const PairedDevice& device) {
        QMetaObject::invokeMethod(qApp, [this, device] {
            if (m_pairingDialog) {
                m_pairingDialog->onPairingComplete(
                    QString::fromStdString(device.deviceName));
            }
        }, Qt::QueuedConnection);
    });

    m_adb = std::make_unique<AdbManager>(45678, 45679); // 45679 = porta stream TCP futura
    if (!m_adb->init()) {
        qWarning() << "AdbManager: falha na inicialização";
        // não fatal — modo WiFi ainda funciona
    }
    m_adb->setOnDeviceAttached([this](const AdbDevice& dev) {
        QMetaObject::invokeMethod(qApp, [this, dev] {
            qDebug() << "USB conectado:" << dev.serial.c_str();
            m_tray->showNotification("Iriseus",
                "Celular conectado via USB: " + QString::fromStdString(dev.serial));
        }, Qt::QueuedConnection);
    });
    m_adb->setOnDeviceDetached([this](const std::string& serial) {
        QMetaObject::invokeMethod(qApp, [this, serial] {
            qDebug() << "USB desconectado:" << serial.c_str();
        }, Qt::QueuedConnection);
    });
    m_adb->start();

    m_stream = std::make_unique<StreamReceiver>(45679, 1280, 720, 30.0f);
    if (!m_stream->start()) {
        qWarning() << "StreamReceiver: falha ao iniciar — Softcam registrado?";
        // não fatal — app sobe sem câmera virtual até Softcam ser registrado
    }
    m_stream->setOnStatusChange([this](bool active) {
        QMetaObject::invokeMethod(qApp, [this, active] {
            m_tray->updateCameraStatus(active);
        }, Qt::QueuedConnection);
    });

    m_tray = std::make_unique<TrayIcon>();
    m_tray->show();

    QObject::connect(m_tray.get(), &TrayIcon::pairRequested, qApp, [this] {
        auto offer      = m_pairing->createOffer();
        m_pairingDialog = new PairingDialog(offer);
        m_pairingDialog->exec();
        m_pairingDialog = nullptr;
    });

    QObject::connect(qApp, &QApplication::aboutToQuit, qApp, [this] {
        m_stream->stop();
        m_adb->stop();
        m_mdns->stop();
        m_wsServer->stop();
    });

    return true;
}

void Application::setupServerCallbacks()
{
    ServerCallbacks cb;

    cb.onDeviceConnected = [](uint64_t id, const std::string& name) {
        qDebug() << "Dispositivo conectado:" << QString::fromStdString(name) << "id=" << id;
    };
    cb.onDeviceDisconnected = [](uint64_t id) {
        qDebug() << "Dispositivo desconectado id=" << id;
    };
    cb.onStartCamera = [](uint64_t /*id*/) {
        qDebug() << "StartCamera solicitado";
    };
    cb.onStopCamera = [](uint64_t /*id*/) {
        qDebug() << "StopCamera solicitado";
    };
    cb.onPairRequest = [this](uint64_t sessionId, const json& payload) {
        auto pin        = payload.value("pin",        "");
        auto peerPubB64 = payload.value("pk",         "");
        auto deviceId   = payload.value("deviceId",   "");
        auto deviceName = payload.value("deviceName", "");

        bool ok = m_pairing->handlePairRequest(pin, peerPubB64, deviceId, deviceName);
        json reply = {{"type", ok ? "pair_accepted" : "pair_rejected"}};
        m_wsServer->sendTo(sessionId, reply.dump());
    };
    cb.getPcPublicKey = [this]() -> std::string {
        if (!m_pairing) return "";
        auto offer = m_pairing->currentOffer();
        return offer.pubKeyB64;
    };

    m_wsServer->setCallbacks(std::move(cb));
}
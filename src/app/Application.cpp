#include "Application.h"
#include "ui/TrayIcon.h"
#include "core/WsServer.h"
#include "core/PairingManager.h"
#include "ui/PairingDialog.h"
#include <QApplication>
#include <QDebug>
#include <QSettings>
#include <QDesktopServices>
#include <QFileDialog>
#include <QUrl>
#include "core/AdbManager.h"
#include "core/StreamReceiver.h"
#include "core/FileTransferManager.h"

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

    m_pairing = std::make_unique<PairingManager>(45678);
    m_pairing->setOnPaired([this](const PairedDevice& device) {
        // Persiste dispositivo pareado
        QSettings settings("Iriseus", "Iriseus");
        settings.setValue("pairedDeviceId",   QString::fromStdString(device.deviceId));
        settings.setValue("pairedDeviceName", QString::fromStdString(device.deviceName));

        QMetaObject::invokeMethod(qApp, [this, device] {
            if (m_pairingDialog) {
                m_pairingDialog->onPairingComplete(
                    QString::fromStdString(device.deviceName));
            }
        }, Qt::QueuedConnection);
    });

    // Restaura dispositivo pareado anterior (se houver)
    QSettings settings("Iriseus", "Iriseus");
    auto savedId   = settings.value("pairedDeviceId").toString().toStdString();
    auto savedName = settings.value("pairedDeviceName").toString().toStdString();
    if (!savedId.empty()) {
        m_pairing->restoreDevice(savedId, savedName);
    }

    m_adb = std::make_unique<AdbManager>(45678, 45679, 45680);
    qDebug() << "AdbManager: iniciando init()";
    if (!m_adb->init()) {
        qWarning() << "AdbManager: falha na inicialização";
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
    qDebug() << "AdbManager: init() concluído, iniciando start()";
    m_adb->start();
    qDebug() << "AdbManager: start() concluído";

    m_stream = std::make_unique<StreamReceiver>(45679, 0, 0, 30.0f);
    if (!m_stream->start()) {
        qWarning() << "StreamReceiver: falha ao iniciar — Softcam registrado?";
    }
    m_stream->setOnStatusChange([this](bool active) {
        QMetaObject::invokeMethod(qApp, [this, active] {
            m_tray->updateCameraStatus(active);
        }, Qt::QueuedConnection);
    });

    m_fileManager = std::make_unique<Iriseus::FileTransferManager>(m_wsServer.get(), m_pairing.get());
    if (!m_fileManager->start()) {
        qWarning() << "FileTransferManager: falha ao iniciar servidor de arquivos na porta 45680";
    }

    QObject::connect(m_fileManager.get(), &Iriseus::FileTransferManager::transferFinished,
                     qApp, [this](QString transferId, QString fileName, bool success, QString error) {
        Q_UNUSED(transferId)
        if (success) {
            m_tray->showNotification("Iriseus", "Arquivo recebido com sucesso: " + fileName);
        } else {
            m_tray->showNotification("Iriseus", "Falha ao receber " + fileName + ": " + error);
        }
    });

    m_tray = std::make_unique<TrayIcon>();
    m_tray->show();

    QObject::connect(m_tray.get(), &TrayIcon::pairRequested, qApp, [this] {
        auto offer      = m_pairing->createOffer();
        m_pairingDialog = new PairingDialog(offer);
        m_pairingDialog->exec();
        m_pairingDialog = nullptr;
    });

    QObject::connect(m_tray.get(), &TrayIcon::sendFilesRequested, qApp, [this] {
        QStringList files = QFileDialog::getOpenFileNames(nullptr, "Selecionar arquivos para enviar ao celular");
        if (!files.isEmpty() && m_fileManager) {
            m_fileManager->sendFiles(0, files);
        }
    });

    QObject::connect(m_tray.get(), &TrayIcon::openDownloadsRequested, qApp, [this] {
        QString dirPath = m_fileManager->defaultDownloadDir();
        QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
    });

    QObject::connect(qApp, &QApplication::aboutToQuit, qApp, [this] {
        m_fileManager->stop();
        m_stream->stop();
        m_adb->stop();
        m_wsServer->stop();
    });

    return true;
}

void Application::setupServerCallbacks()
{
    ServerCallbacks cb;

    cb.onDeviceConnected = [this](uint64_t id, const std::string& name) {
        qDebug() << "Dispositivo conectado:" << QString::fromStdString(name) << "id=" << id;
        if (m_fileManager) m_fileManager->setCurrentSession(id);
    };
    cb.onDeviceDisconnected = [this](uint64_t id) {
        qDebug() << "Dispositivo desconectado id=" << id;
        if (m_fileManager && m_fileManager->currentSession() == id) {
            m_fileManager->setCurrentSession(0);
        }
    };
    cb.onStartCamera = [this](uint64_t /*id*/) {
        qDebug() << "StartCamera solicitado";
        QMetaObject::invokeMethod(qApp, [this] {
            if (m_tray) m_tray->updateStreamMode(false);
        }, Qt::QueuedConnection);
    };
    cb.onStopCamera = [this](uint64_t /*id*/) {
        qDebug() << "StopCamera solicitado";
    };
    cb.onStartScreen = [this](uint64_t /*id*/) {
        qDebug() << "StartScreen solicitado";
        QMetaObject::invokeMethod(qApp, [this] {
            if (m_tray) m_tray->updateStreamMode(true);
        }, Qt::QueuedConnection);
    };
    cb.onStopScreen = [this](uint64_t /*id*/) {
        qDebug() << "StopScreen solicitado";
        QMetaObject::invokeMethod(qApp, [this] {
            if (m_tray) m_tray->updateStreamMode(false);
        }, Qt::QueuedConnection);
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
    cb.onFileOffer = [this](uint64_t sessionId, const json& payload) {
        if (m_fileManager) m_fileManager->handleFileOffer(sessionId, payload);
    };
    cb.onFileAccept = [this](uint64_t sessionId, const json& payload) {
        if (m_fileManager) m_fileManager->handleFileAccept(sessionId, payload);
    };
    cb.onFileReject = [this](uint64_t sessionId, const json& payload) {
        if (m_fileManager) m_fileManager->handleFileReject(sessionId, payload);
    };
    cb.onFileComplete = [this](uint64_t sessionId, const json& payload) {
        if (m_fileManager) m_fileManager->handleFileComplete(sessionId, payload);
    };
    cb.onFileCancel = [this](uint64_t sessionId, const json& payload) {
        if (m_fileManager) m_fileManager->handleFileCancel(sessionId, payload);
    };
    cb.getPcPublicKey = [this]() -> std::string {
        if (!m_pairing) return "";
        auto offer = m_pairing->currentOffer();
        return offer.pubKeyB64;
    };

    m_wsServer->setCallbacks(std::move(cb));
}
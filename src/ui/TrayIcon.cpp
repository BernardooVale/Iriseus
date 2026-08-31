#include "TrayIcon.h"
#include <QApplication>
#include <QIcon>
#include <QStyle>
#include <QMessageBox>

TrayIcon::TrayIcon(QObject* parent)
    : QObject(parent)
{
    buildMenu();

    m_tray = std::make_unique<QSystemTrayIcon>(this);
    m_tray->setContextMenu(m_menu.get());
    m_tray->setToolTip("Iriseus");

    updateIcon(false);

    connect(m_tray.get(), &QSystemTrayIcon::activated,
            this, &TrayIcon::onActivated);
}

TrayIcon::~TrayIcon() = default;

void TrayIcon::show()
{
    m_tray->show();
}

void TrayIcon::buildMenu()
{
    m_menu = std::make_unique<QMenu>();

    // Label de status (não clicável)
    QAction* statusLabel = m_menu->addAction("Iriseus");
    statusLabel->setEnabled(false);
    m_menu->addSeparator();

    m_actionStartCamera = m_menu->addAction("Iniciar câmera");
    m_actionStopCamera  = m_menu->addAction("Parar câmera");
    m_actionPair        = m_menu->addAction("Parear novo dispositivo...");
    m_actionStopCamera->setVisible(false);

    m_menu->addSeparator();
    QAction* actionQuit = m_menu->addAction("Sair");

    connect(m_actionStartCamera,    &QAction::triggered, this, &TrayIcon::onStartCamera);
    connect(m_actionStopCamera,     &QAction::triggered, this, &TrayIcon::onStopCamera);
    connect(actionQuit,             &QAction::triggered, this, &TrayIcon::onQuit);
    connect(m_actionPair,           &QAction::triggered, this, &TrayIcon::onPairDevice);
}

void TrayIcon::updateIcon(bool active)
{
    // Por ora usa ícone genérico do Qt — substituir por Iriseus.ico / Iriseus_active.ico
    QStyle::StandardPixmap px = active
        ? QStyle::SP_MediaPlay
        : QStyle::SP_ComputerIcon;
    m_tray->setIcon(QApplication::style()->standardIcon(px));
    m_cameraActive = active;
}

void TrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    // Clique duplo abre menu — comportamento padrão já funciona com setContextMenu
    Q_UNUSED(reason)
}

void TrayIcon::onStartCamera()
{
    // TODO: fase 2 — iniciar pipeline FFmpeg → Softcam
    m_actionStartCamera->setVisible(false);
    m_actionStopCamera->setVisible(true);
    updateIcon(true);
    m_tray->showMessage("Iriseus", "Câmera iniciada", QSystemTrayIcon::Information, 2000);
}

void TrayIcon::onStopCamera()
{
    // TODO: fase 2 — parar pipeline
    m_actionStartCamera->setVisible(true);
    m_actionStopCamera->setVisible(false);
    updateIcon(false);
    m_tray->showMessage("Iriseus", "Câmera parada", QSystemTrayIcon::Information, 2000);
}

void TrayIcon::onQuit()
{
    m_tray->hide();
    QApplication::quit();
}

void TrayIcon::showNotification(const QString& title, const QString& message)
{
    m_tray->showMessage(title, message, QSystemTrayIcon::Information, 3000);
}

void TrayIcon::onPairDevice()
{
    emit pairRequested();
}
#pragma once
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <memory>

class TrayIcon : public QObject
{
    Q_OBJECT

public:
    explicit TrayIcon(QObject* parent = nullptr);
    ~TrayIcon() override;

    void show();
    void showNotification(const QString& title, const QString& message);
    void updateCameraStatus(bool active);
    void updateStreamMode(bool isScreen);
    void setFlipHorizontal(bool flip);

signals:
    void pairRequested();
    void openDownloadsRequested();
    void sendFilesRequested();
    void flipHorizontalToggled(bool flip);

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);
    void onStartCamera();
    void onStopCamera();
    void onQuit();
    void onPairDevice();

private:
    void updateIcon(bool active);
    void buildMenu();

    std::unique_ptr<QSystemTrayIcon>    m_tray;
    std::unique_ptr<QMenu>              m_menu;
    QAction*                            m_actionStartCamera    = nullptr;
    QAction*                            m_actionStopCamera     = nullptr;
    QAction*                            m_actionPair           = nullptr;
    QAction*                            m_actionFlipHorizontal = nullptr;

    bool m_cameraActive = false;
};

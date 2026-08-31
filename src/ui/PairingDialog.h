#pragma once
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QString>

struct PairingOffer;

// Janela de pareamento — exibe QR Code e PIN
// Aberta via menu do systray "Parear novo dispositivo"

class PairingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PairingDialog(const PairingOffer& offer, QWidget* parent = nullptr);

    // Chamado pelo Application quando pareamento completo — fecha a janela
    void onPairingComplete(const QString& deviceName);

private:
    void buildQrPixmap(const std::string& payload);

    QLabel*      m_qrLabel   = nullptr;
    QLabel*      m_pinLabel  = nullptr;
    QLabel*      m_statusLabel = nullptr;
    QPushButton* m_cancelBtn = nullptr;
};
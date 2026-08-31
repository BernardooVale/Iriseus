#include "PairingDialog.h"
#include "core/PairingManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QImage>
#include <QPainter>
#include <QFont>
#include "qrcodegen.hpp"
#include <QTimer>

using qrcodegen::QrCode;

PairingDialog::PairingDialog(const PairingOffer& offer, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Parear dispositivo — DevLink");
    setFixedSize(320, 420);

    auto* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(12);

    // QR Code
    m_qrLabel = new QLabel(this);
    m_qrLabel->setAlignment(Qt::AlignCenter);
    buildQrPixmap(PairingManager::buildQrPayload(offer));
    layout->addWidget(m_qrLabel);

    // PIN
    auto* pinTitle = new QLabel("ou use o PIN:", this);
    pinTitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(pinTitle);

    m_pinLabel = new QLabel(QString::fromStdString(offer.pin), this);
    QFont pinFont = m_pinLabel->font();
    pinFont.setPointSize(28);
    pinFont.setBold(true);
    m_pinLabel->setFont(pinFont);
    m_pinLabel->setAlignment(Qt::AlignCenter);
    QFont pinFontSpaced = m_pinLabel->font();
    pinFontSpaced.setLetterSpacing(QFont::AbsoluteSpacing, 6);
    m_pinLabel->setFont(pinFontSpaced);
    layout->addWidget(m_pinLabel);

    // Status
    m_statusLabel = new QLabel("Aguardando conexão do celular...", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statusLabel);

    // Botão cancelar
    m_cancelBtn = new QPushButton("Cancelar", this);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    layout->addWidget(m_cancelBtn);
}

void PairingDialog::onPairingComplete(const QString& deviceName)
{
    m_statusLabel->setText("✓ Pareado com " + deviceName);
    m_cancelBtn->setText("Fechar");
    m_qrLabel->setEnabled(false);
    m_pinLabel->setEnabled(false);

    // Fecha automaticamente após 2s
    QTimer::singleShot(2000, this, &QDialog::accept);
}

void PairingDialog::buildQrPixmap(const std::string& payload)
{
    auto qr = QrCode::encodeText(payload.c_str(), QrCode::Ecc::MEDIUM);

    int scale   = 6;
    int modules = qr.getSize();
    int imgSize = modules * scale;

    QImage img(imgSize, imgSize, QImage::Format_RGB32);
    img.fill(Qt::white);

    QPainter painter(&img);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);

    for (int y = 0; y < modules; ++y)
        for (int x = 0; x < modules; ++x)
            if (qr.getModule(x, y))
                painter.drawRect(x * scale, y * scale, scale, scale);

    m_qrLabel->setPixmap(QPixmap::fromImage(img));
}
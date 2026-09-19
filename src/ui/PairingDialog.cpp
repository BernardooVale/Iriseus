#include "PairingDialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFont>
#include <QTimer>

PairingDialog::PairingDialog(const PairingOffer& offer, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Parear dispositivo — Iriseus");
    setFixedSize(280, 260);

    auto* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 24, 24, 24);

    // Instrução
    auto* instrLabel = new QLabel("Abra o Iriseus no celular e\ndigite o código abaixo:", this);
    instrLabel->setAlignment(Qt::AlignCenter);
    instrLabel->setWordWrap(true);
    layout->addWidget(instrLabel);

    // PIN em destaque
    m_pinLabel = new QLabel(formatPin(offer.pin), this);
    QFont pinFont = m_pinLabel->font();
    pinFont.setPointSize(36);
    pinFont.setBold(true);
    pinFont.setLetterSpacing(QFont::AbsoluteSpacing, 8);
    m_pinLabel->setFont(pinFont);
    m_pinLabel->setAlignment(Qt::AlignCenter);
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
    m_pinLabel->setEnabled(false);
    QTimer::singleShot(2000, this, &QDialog::accept);
}

QString PairingDialog::formatPin(const std::string& pin)
{
    // Formata como "123 456" para facilitar leitura
    if (pin.size() == 6)
        return QString::fromStdString(pin.substr(0, 3) + " " + pin.substr(3, 3));
    return QString::fromStdString(pin);
}
#pragma once
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <string>
#include "core/PairingManager.h"

class PairingDialog : public QDialog {
    Q_OBJECT
public:
    explicit PairingDialog(const PairingOffer& offer, QWidget* parent = nullptr);
    void onPairingComplete(const QString& deviceName);

private:
    static QString formatPin(const std::string& pin);

    QLabel*      m_pinLabel    = nullptr;
    QLabel*      m_statusLabel = nullptr;
    QPushButton* m_cancelBtn   = nullptr;
};
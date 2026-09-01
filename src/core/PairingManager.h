#pragma once
#include "PairingCrypto.h"
#include <string>
#include <functional>
#include <unordered_map>
#include <cstdint>

struct PairedDevice {
    std::string              deviceId;
    std::string              deviceName;
    std::array<uint8_t, 32> peerPubKey;
    SharedSecret             sharedSecret;
};

struct PairingOffer {
    std::string pcIp;
    uint16_t    wsPort;
    std::string pin;                    // 6 dígitos
    std::string pubKeyB64;              // chave pública do PC em base64
    KeyPair     localKeyPair;           // par efêmero desta sessão
};

class PairingManager
{
public:
    using OnPaired = std::function<void(const PairedDevice&)>;

    explicit PairingManager(uint16_t wsPort);

    // Gera nova oferta de pareamento (novo QR / PIN)
    PairingOffer createOffer();
    const PairingOffer& currentOffer() const { return m_currentOffer; }

    // Chamado pelo WsSession ao receber mensagem "pair_request"
    // Retorna true se PIN válido e completa o pareamento
    bool handlePairRequest(const std::string& pin,
                           const std::string& peerPubKeyB64,
                           const std::string& deviceId,
                           const std::string& deviceName);

    bool isPaired(const std::string& deviceId) const;
    const PairedDevice* getDevice(const std::string& deviceId) const;

    void setOnPaired(OnPaired cb) { m_onPaired = std::move(cb); }

    // QR payload — JSON serializado para encode no QRCode
    static std::string buildQrPayload(const PairingOffer& offer);

private:
    std::string generatePin();
    std::string getLocalIp();

    uint16_t                                    m_wsPort;
    PairingCrypto                               m_crypto;
    std::unordered_map<std::string, PairedDevice> m_devices;
    std::string                                 m_activePin;
    KeyPair                                     m_activeKeyPair;
    PairingOffer                                m_currentOffer;
    OnPaired                                    m_onPaired;
};
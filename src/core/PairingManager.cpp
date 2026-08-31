#include "PairingManager.h"
#include <nlohmann/json.hpp>
#include <sodium.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>

using json = nlohmann::json;

PairingManager::PairingManager(uint16_t wsPort)
    : m_wsPort(wsPort)
{}

PairingOffer PairingManager::createOffer()
{
    m_activeKeyPair = m_crypto.generateKeyPair();
    m_activePin     = generatePin();

    PairingOffer offer;
    offer.pcIp      = getLocalIp();
    offer.wsPort    = m_wsPort;
    offer.pin       = m_activePin;
    offer.pubKeyB64 = PairingCrypto::toBase64(
        m_activeKeyPair.pub.data(), m_activeKeyPair.pub.size());
    offer.localKeyPair = m_activeKeyPair;
    return offer;
}

bool PairingManager::handlePairRequest(const std::string& pin,
                                        const std::string& peerPubKeyB64,
                                        const std::string& deviceId,
                                        const std::string& deviceName)
{
    // Valida PIN com comparação em tempo constante
    if (pin.size() != m_activePin.size()) return false;
    if (sodium_memcmp(pin.data(), m_activePin.data(), pin.size()) != 0)
        return false;

    auto peerPubVec = PairingCrypto::fromBase64(peerPubKeyB64);
    if (peerPubVec.size() != 32) return false;

    std::array<uint8_t, 32> peerPub;
    std::copy(peerPubVec.begin(), peerPubVec.end(), peerPub.begin());

    PairedDevice device;
    device.deviceId     = deviceId;
    device.deviceName   = deviceName;
    device.peerPubKey   = peerPub;
    device.sharedSecret = m_crypto.deriveShared(m_activeKeyPair, peerPub);

    m_devices[deviceId] = device;

    // Invalida PIN após uso — TOFU
    m_activePin.clear();

    if (m_onPaired) m_onPaired(device);
    return true;
}

bool PairingManager::isPaired(const std::string& deviceId) const
{
    return m_devices.count(deviceId) > 0;
}

const PairedDevice* PairingManager::getDevice(const std::string& deviceId) const
{
    auto it = m_devices.find(deviceId);
    return it != m_devices.end() ? &it->second : nullptr;
}

std::string PairingManager::buildQrPayload(const PairingOffer& offer)
{
    json payload = {
        {"v",    1},
        {"ip",   offer.pcIp},
        {"port", offer.wsPort},
        {"pin",  offer.pin},
        {"pk",   offer.pubKeyB64}
    };
    return payload.dump();
}

std::string PairingManager::generatePin()
{
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> dist(0, 999999);
    std::ostringstream oss;
    oss << std::setw(6) << std::setfill('0') << dist(rng);
    return oss.str();
}

std::string PairingManager::getLocalIp()
{
    // Pega primeiro IP não-loopback da máquina
    char hostname[256];
    gethostname(hostname, sizeof(hostname));

    addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    getaddrinfo(hostname, nullptr, &hints, &res);

    std::string ip = "127.0.0.1";
    if (res) {
        auto* addr = reinterpret_cast<sockaddr_in*>(res->ai_addr);
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr->sin_addr, buf, sizeof(buf));
        ip = buf;
        freeaddrinfo(res);
    }
    return ip;
}
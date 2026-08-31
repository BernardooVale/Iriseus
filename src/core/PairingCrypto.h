#pragma once
#include <array>
#include <string>
#include <cstdint>
#include <vector>

// X25519 ECDH + derivação de chave via libsodium
// Gera par de chaves efêmero por sessão de pareamento

struct KeyPair {
    std::array<uint8_t, 32> pub;
    std::array<uint8_t, 32> priv;
};

struct SharedSecret {
    std::array<uint8_t, 32> key; // chave simétrica derivada
};

class PairingCrypto
{
public:
    PairingCrypto();

    // Gera par efêmero — chamado ao iniciar pareamento
    KeyPair generateKeyPair();

    // Deriva segredo compartilhado a partir da chave pública do peer
    SharedSecret deriveShared(const KeyPair& local,
                              const std::array<uint8_t, 32>& peerPub);

    // Encode/decode base64 para trafegar chave pública no QR / WebSocket
    static std::string toBase64(const uint8_t* data, size_t len);
    static std::vector<uint8_t> fromBase64(const std::string& b64);
};
#include "PairingCrypto.h"
#include <sodium.h>
#include <stdexcept>
#include <vector>

PairingCrypto::PairingCrypto()
{
    if (sodium_init() < 0)
        throw std::runtime_error("libsodium init failed");
}

KeyPair PairingCrypto::generateKeyPair()
{
    KeyPair kp;
    crypto_kx_keypair(kp.pub.data(), kp.priv.data());
    return kp;
}

SharedSecret PairingCrypto::deriveShared(const KeyPair& local,
                                          const std::array<uint8_t, 32>& peerPub)
{
    SharedSecret secret;
    // Usamos crypto_kx_server_session_keys — PC é sempre "server" no pareamento
    std::array<uint8_t, 32> rx, tx;
    if (crypto_kx_server_session_keys(
            rx.data(), tx.data(),
            local.pub.data(), local.priv.data(),
            peerPub.data()) != 0)
    {
        throw std::runtime_error("ECDH key derivation failed");
    }
    // XOR rx+tx → chave bidirecional única (simples e seguro para canal local)
    for (size_t i = 0; i < 32; ++i)
        secret.key[i] = rx[i] ^ tx[i];
    return secret;
}

std::string PairingCrypto::toBase64(const uint8_t* data, size_t len)
{
    size_t b64Len = sodium_base64_encoded_len(len, sodium_base64_VARIANT_URLSAFE_NO_PADDING);
    std::string out(b64Len, '\0');
    sodium_bin2base64(out.data(), b64Len, data, len,
                      sodium_base64_VARIANT_URLSAFE_NO_PADDING);
    // sodium inclui null terminator no tamanho — remove
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

std::vector<uint8_t> PairingCrypto::fromBase64(const std::string& b64)
{
    size_t binMaxLen = b64.size();
    std::vector<uint8_t> bin(binMaxLen);
    size_t binLen = 0;
    const char* b64End = nullptr;

    if (sodium_base642bin(
            bin.data(), binMaxLen,
            b64.data(), b64.size(),
            nullptr, &binLen, &b64End,
            sodium_base64_VARIANT_URLSAFE_NO_PADDING) != 0)
    {
        throw std::runtime_error("base64 decode failed");
    }
    bin.resize(binLen);
    return bin;
}
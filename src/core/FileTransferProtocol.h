#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include <nlohmann/json.hpp>

namespace Iriseus {

constexpr uint16_t kFileTransferPort = 45680;
constexpr size_t   kChunkPlaintextSize = 256 * 1024; // 256 KiB
constexpr size_t   kSecretStreamTagLen = 17;        // libsodium crypto_secretstream_xchacha20poly1305_ABYTES (17)
constexpr size_t   kChunkCiphermaxSize = kChunkPlaintextSize + kSecretStreamTagLen;
constexpr size_t   kTokenSize = 32;                 // 32 bytes token
constexpr size_t   kUuidBinarySize = 16;            // 16 bytes UUID
constexpr int      kFileTransferTimeoutSec = 30;

// Mensagens WS
struct FileOffer {
    std::string transferId;
    std::string name;
    uint64_t    size = 0;
    std::string mime;
    std::string sha256;
};

inline void to_json(nlohmann::json& j, const FileOffer& p) {
    j = nlohmann::json{
        {"type",       "file_offer"},
        {"transferId", p.transferId},
        {"name",       p.name},
        {"size",       p.size},
        {"mime",       p.mime},
        {"sha256",     p.sha256}
    };
}

inline void from_json(const nlohmann::json& j, FileOffer& p) {
    p.transferId = j.value("transferId", "");
    p.name       = j.value("name", "");
    p.size       = j.value("size", uint64_t(0));
    p.mime       = j.value("mime", "");
    p.sha256     = j.value("sha256", "");
}

// Handshake TCP (49 bytes no total inicial enviado pelo mobile cliente):
// [16 bytes transferId (binário ou raw/hex)]
// [32 bytes token]
// [1 byte dir: 0 = mobile envia -> PC recebe, 1 = desktop envia -> mobile recebe]
enum class TransferDirection : uint8_t {
    MobileUploadToPc = 0,
    DesktopDownloadToMobile = 1
};

struct FileTransferHandshake {
    uint8_t transferId[kUuidBinarySize];
    uint8_t token[kTokenSize];
    TransferDirection direction;
};

enum class TransferStatus {
    Pending,
    Transferring,
    Completed,
    Failed,
    Cancelled
};

} // namespace Iriseus

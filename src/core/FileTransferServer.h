#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <boost/asio.hpp>
#include "FileTransferProtocol.h"

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

namespace Iriseus {

struct ActiveTransferSession {
    std::string transferId;
    std::string tokenBase64;
    std::vector<uint8_t> tokenBytes;
    std::string targetFilePath; // Caminho final ou base
    std::string tempFilePath;   // .part
    uint64_t expectedSize = 0;
    std::string expectedSha256;
    TransferDirection direction = TransferDirection::MobileUploadToPc;
    std::atomic<uint64_t> bytesTransferred{0};
    std::atomic<bool> cancelled{false};
};

class FileTransferServer {
public:
    using ProgressCallback = std::function<void(const std::string& transferId, uint64_t bytesTransferred, uint64_t totalBytes)>;
    using FinishedCallback = std::function<void(const std::string& transferId, bool success, const std::string& error)>;

    explicit FileTransferServer(uint16_t port = kFileTransferPort);
    ~FileTransferServer();

    bool start();
    void stop();

    // Registra uma transferência autorizada para aguardar conexão TCP
    void registerTransfer(const std::string& transferId,
                          const std::string& tokenBase64,
                          const std::vector<uint8_t>& tokenBytes,
                          const std::string& targetPath,
                          uint64_t expectedSize,
                          const std::string& expectedSha256,
                          TransferDirection direction);

    void cancelTransfer(const std::string& transferId);

    void setOnProgress(ProgressCallback cb) { m_onProgress = std::move(cb); }
    void setOnFinished(FinishedCallback cb) { m_onFinished = std::move(cb); }

private:
    void acceptLoop();
    void handleConnection(tcp::socket socket);
    bool readExact(tcp::socket& socket, uint8_t* buf, size_t size);

    uint16_t m_port;
    asio::io_context m_ioc;
    tcp::acceptor    m_acceptor;
    std::atomic<bool> m_running{false};
    std::thread m_thread;

    std::mutex m_sessionsMutex;
    std::unordered_map<std::string, std::shared_ptr<ActiveTransferSession>> m_sessions;

    ProgressCallback m_onProgress;
    FinishedCallback m_onFinished;
};

} // namespace Iriseus

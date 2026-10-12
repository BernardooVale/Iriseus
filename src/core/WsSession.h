#pragma once
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <memory>
#include <string>
#include <functional>
#include <deque>
#include <mutex>
#include <boost/beast/core/tcp_stream.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace beast = boost::beast;
namespace ws    = beast::websocket;
namespace asio  = boost::asio;
using tcp       = asio::ip::tcp;

// Callbacks que WsServer registra para reagir a eventos de sessão
struct SessionCallbacks {
    std::function<void(uint64_t sessionId, const std::string& deviceName)> onHello;
    std::function<void(uint64_t sessionId)> onStartCamera;
    std::function<void(uint64_t sessionId)> onStopCamera;
    std::function<void(uint64_t sessionId)> onDisconnect;
    std::function<void(uint64_t sessionId, const json& payload)> onPairRequest;
    std::function<void(uint64_t sessionId, const json& payload)> onFileOffer;
    std::function<void(uint64_t sessionId, const json& payload)> onFileAccept;
    std::function<void(uint64_t sessionId, const json& payload)> onFileReject;
    std::function<void(uint64_t sessionId, const json& payload)> onFileComplete;
    std::function<void(uint64_t sessionId, const json& payload)> onFileCancel;
    std::function<std::string()> getPcPublicKey;
};

class WsSession : public std::enable_shared_from_this<WsSession>
{
public:
    WsSession(tcp::socket socket,
              uint64_t sessionId,
              SessionCallbacks callbacks);

    void start();
    void send(const std::string& message);
    void close();

    uint64_t id() const { return m_id; }

private:
    void doRead();
    void onRead(beast::error_code ec, std::size_t bytesRead);
    void doWrite();
    void handleMessage(const std::string& raw);

    ws::stream<beast::tcp_stream> m_ws;
    beast::flat_buffer            m_buffer;
    uint64_t                      m_id;
    SessionCallbacks              m_callbacks;

    std::mutex                    m_writeMutex;
    std::deque<std::string>       m_writeQueue;
};
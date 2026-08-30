#pragma once
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <memory>
#include <string>
#include <functional>
#include <boost/beast/core/tcp_stream.hpp>

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
    void doWrite(const std::string& message);
    void handleMessage(const std::string& raw);

    ws::stream<beast::tcp_stream> m_ws;
    beast::flat_buffer            m_buffer;
    uint64_t                      m_id;
    SessionCallbacks              m_callbacks;
};
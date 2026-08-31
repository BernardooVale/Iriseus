#pragma once
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <thread>
#include <unordered_map>
#include <atomic>
#include <functional>
#include "WsSession.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace asio = boost::asio;
using tcp      = asio::ip::tcp;

struct ServerCallbacks {
    std::function<void(uint64_t sessionId, const std::string& deviceName)> onDeviceConnected;
    std::function<void(uint64_t sessionId)> onDeviceDisconnected;
    std::function<void(uint64_t sessionId)> onStartCamera;
    std::function<void(uint64_t sessionId)> onStopCamera;
    std::function<void(uint64_t sessionId, const json& payload)> onPairRequest;
};

class WsServer
{
public:
    explicit WsServer(uint16_t port = 45678);
    ~WsServer();

    void setCallbacks(ServerCallbacks cb);
    bool start();
    void stop();
    void sendTo(uint64_t sessionId, const std::string& message);

    bool isRunning() const { return m_running.load(); }

private:
    void doAccept();
    void removeSession(uint64_t sessionId);

    uint16_t                                          m_port;
    asio::io_context                                  m_ioc;
    tcp::acceptor                                     m_acceptor;
    std::thread                                       m_thread;
    std::atomic<bool>                                 m_running{false};
    std::atomic<uint64_t>                             m_nextId{1};
    std::unordered_map<uint64_t, std::shared_ptr<WsSession>> m_sessions;
    ServerCallbacks                                   m_callbacks;
};
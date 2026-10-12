#include "WsSession.h"
#include "MessageDispatcher.h"
#include <nlohmann/json.hpp>
#include <boost/beast/core.hpp>

WsSession::WsSession(tcp::socket socket,
                     uint64_t sessionId,
                     SessionCallbacks callbacks)
    : m_ws(std::move(socket))
    , m_id(sessionId)
    , m_callbacks(std::move(callbacks))
{}

void WsSession::start()
{
    // Handshake WebSocket assíncrono
    m_ws.async_accept([self = shared_from_this()](beast::error_code ec) {
        if (ec) return;
        self->doRead();
    });
}

void WsSession::doRead()
{
    m_buffer.clear();
    m_ws.async_read(m_buffer,
        [self = shared_from_this()](beast::error_code ec, std::size_t bytes) {
            self->onRead(ec, bytes);
        });
}

void WsSession::onRead(beast::error_code ec, std::size_t /*bytesRead*/)
{
    if (ec == ws::error::closed || ec == beast::error::timeout) {
        if (m_callbacks.onDisconnect) m_callbacks.onDisconnect(m_id);
        return;
    }
    if (ec) {
        if (m_callbacks.onDisconnect) m_callbacks.onDisconnect(m_id);
        return;
    }

    auto raw = beast::buffers_to_string(m_buffer.data());
    handleMessage(raw);
    doRead(); // loop de leitura
}

void WsSession::handleMessage(const std::string& raw)
{
    auto msg = parseMessage(raw);

    switch (msg.type) {
    case MsgType::Hello: {
        auto deviceName = msg.payload.value("deviceName", "Unknown");
        if (m_callbacks.onHello) m_callbacks.onHello(m_id, deviceName);

        // Responde com Welcome
        json reply = {
            {"type",       "welcome"},
            {"deviceName", "Iriseus PC"},
            {"deviceId",   "pc-placeholder-id"},
            {"pk",         m_callbacks.getPcPublicKey ? m_callbacks.getPcPublicKey() : ""}
        };
        send(reply.dump());
        break;
    }
    case MsgType::StartCamera:
        if (m_callbacks.onStartCamera) m_callbacks.onStartCamera(m_id);
        break;

    case MsgType::StopCamera:
        if (m_callbacks.onStopCamera) m_callbacks.onStopCamera(m_id);
        break;

    case MsgType::Ping: {
        json pong = {{"type", "pong"}};
        send(pong.dump());
        break;
    }
    case MsgType::PairRequest: {
        // WsSession não tem acesso direto ao PairingManager
        // delega via callback genérico
        if (m_callbacks.onPairRequest)
            m_callbacks.onPairRequest(m_id, msg.payload);
        break;
    }
    case MsgType::FileOffer: {
        if (m_callbacks.onFileOffer)
            m_callbacks.onFileOffer(m_id, msg.payload);
        break;
    }
    case MsgType::FileAccept: {
        if (m_callbacks.onFileAccept)
            m_callbacks.onFileAccept(m_id, msg.payload);
        break;
    }
    case MsgType::FileReject: {
        if (m_callbacks.onFileReject)
            m_callbacks.onFileReject(m_id, msg.payload);
        break;
    }
    case MsgType::FileComplete: {
        if (m_callbacks.onFileComplete)
            m_callbacks.onFileComplete(m_id, msg.payload);
        break;
    }
    case MsgType::FileCancel: {
        if (m_callbacks.onFileCancel)
            m_callbacks.onFileCancel(m_id, msg.payload);
        break;
    }
    default:
        break;
    }
}

void WsSession::send(const std::string& message)
{
    asio::post(m_ws.get_executor(), [self = shared_from_this(), message]() {
        bool writeInProgress = false;
        {
            std::lock_guard<std::mutex> lk(self->m_writeMutex);
            writeInProgress = !self->m_writeQueue.empty();
            self->m_writeQueue.push_back(message);
        }

        if (!writeInProgress) {
            self->doWrite();
        }
    });
}

void WsSession::doWrite()
{
    std::string msg;
    {
        std::lock_guard<std::mutex> lk(m_writeMutex);
        if (m_writeQueue.empty()) return;
        msg = m_writeQueue.front();
    }

    m_ws.async_write(asio::buffer(msg),
        [self = shared_from_this()](beast::error_code ec, std::size_t) {
            if (ec) return;
            {
                std::lock_guard<std::mutex> lk(self->m_writeMutex);
                if (!self->m_writeQueue.empty()) {
                    self->m_writeQueue.pop_front();
                }
                if (self->m_writeQueue.empty()) {
                    return;
                }
            }
            self->doWrite();
        });
}

void WsSession::close()
{
    beast::error_code ec;
    m_ws.close(ws::close_code::normal, ec);
}
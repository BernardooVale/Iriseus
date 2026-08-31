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
            {"deviceName", "DevLink PC"},
            {"deviceId",   "pc-placeholder-id"}
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
    default:
        break;
    }
}

void WsSession::send(const std::string& message)
{
    m_ws.async_write(asio::buffer(message),
        [self = shared_from_this()](beast::error_code ec, std::size_t) {
            // TODO: fila de escrita se necessário (por ora, mensagens são raras)
            (void)ec;
        });
}

void WsSession::close()
{
    beast::error_code ec;
    m_ws.close(ws::close_code::normal, ec);
}
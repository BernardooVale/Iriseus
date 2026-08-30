#include "WsServer.h"
#include <boost/asio/strand.hpp>

WsServer::WsServer(uint16_t port)
    : m_port(port)
    , m_acceptor(m_ioc)
{}

WsServer::~WsServer()
{
    stop();
}

void WsServer::setCallbacks(ServerCallbacks cb)
{
    m_callbacks = std::move(cb);
}

bool WsServer::start()
{
    try {
        tcp::endpoint endpoint(tcp::v4(), m_port);
        m_acceptor.open(endpoint.protocol());
        m_acceptor.set_option(asio::socket_base::reuse_address(true));
        m_acceptor.bind(endpoint);
        m_acceptor.listen();
    } catch (const std::exception& e) {
        return false;
    }

    m_running = true;
    doAccept();

    // io_context roda em thread dedicada — não bloqueia a thread Qt
    m_thread = std::thread([this] { m_ioc.run(); });
    return true;
}

void WsServer::stop()
{
    if (!m_running.exchange(false)) return;

    m_ioc.stop();
    if (m_thread.joinable()) m_thread.join();

    for (auto& [id, session] : m_sessions)
        session->close();
    m_sessions.clear();
}

void WsServer::doAccept()
{
    m_acceptor.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!m_running) return;
            if (!ec) {
                uint64_t id = m_nextId.fetch_add(1);

                SessionCallbacks scb;
                scb.onHello = [this](uint64_t sid, const std::string& name) {
                    if (m_callbacks.onDeviceConnected)
                        m_callbacks.onDeviceConnected(sid, name);
                };
                scb.onStartCamera = [this](uint64_t sid) {
                    if (m_callbacks.onStartCamera)
                        m_callbacks.onStartCamera(sid);
                };
                scb.onStopCamera = [this](uint64_t sid) {
                    if (m_callbacks.onStopCamera)
                        m_callbacks.onStopCamera(sid);
                };
                scb.onDisconnect = [this](uint64_t sid) {
                    removeSession(sid);
                    if (m_callbacks.onDeviceDisconnected)
                        m_callbacks.onDeviceDisconnected(sid);
                };

                auto session = std::make_shared<WsSession>(
                    std::move(socket), id, std::move(scb));
                m_sessions[id] = session;
                session->start();
            }
            doAccept(); // aceita próxima conexão
        });
}

void WsServer::removeSession(uint64_t sessionId)
{
    m_sessions.erase(sessionId);
}
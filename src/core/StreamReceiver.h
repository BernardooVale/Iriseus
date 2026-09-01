#pragma once
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <thread>
#include <atomic>
#include <functional>
#include <vector>
#include <cstdint>
#include <memory>

class VideoDecoder;
class VirtualCamera;

namespace asio = boost::asio;
using tcp      = asio::ip::tcp;

// Recebe stream H264 via socket TCP
// Protocolo de framing: [4 bytes big-endian tamanho NALU][NALU data]

class StreamReceiver
{
public:
    using OnStatusChange = std::function<void(bool active)>;

    StreamReceiver(uint16_t port, int width, int height, float fps);
    ~StreamReceiver();

    bool start();
    void stop();

    void setOnStatusChange(OnStatusChange cb) { m_onStatus = std::move(cb); }

private:
    void acceptLoop();
    void receiveLoop(tcp::socket socket);
    bool readExact(tcp::socket& socket, uint8_t* buf, size_t size);

    uint16_t m_port;
    int      m_width;
    int      m_height;
    float    m_fps;

    asio::io_context          m_ioc;
    tcp::acceptor             m_acceptor;
    std::thread               m_thread;
    std::atomic<bool>         m_running{false};

    std::unique_ptr<VideoDecoder>  m_decoder;
    std::unique_ptr<VirtualCamera> m_camera;
    OnStatusChange                 m_onStatus;
};
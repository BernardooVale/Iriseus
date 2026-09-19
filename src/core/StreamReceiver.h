#pragma once
#include <cstdint>
#include <thread>
#include <atomic>
#include <memory>
#include <functional>
#include <vector>
#include <boost/asio.hpp>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class VideoDecoder;
class VirtualCamera;

class StreamReceiver {
public:
    StreamReceiver(uint16_t port, int width, int height, float fps);
    ~StreamReceiver();

    bool start();
    void stop();

    void setOnStatusChange(std::function<void(bool)> cb) { m_onStatus = std::move(cb); }

private:
    void acceptLoop();
    void receiveLoop(tcp::socket socket);
    void decodeLoop();
    bool readExact(tcp::socket& socket, uint8_t* buf, size_t size);

    uint16_t m_port;
    int  m_width, m_height;
    float m_fps;

    asio::io_context m_ioc;
    tcp::acceptor    m_acceptor;
    std::atomic<bool> m_running{false};
    std::thread m_netThread;     // recebe dados da rede
    std::thread m_decodeThread;  // decodifica H.264 → RGB

    std::unique_ptr<VideoDecoder>  m_decoder;
    std::unique_ptr<VirtualCamera> m_camera;
    std::function<void(bool)>      m_onStatus;
};
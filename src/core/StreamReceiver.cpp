#include "StreamReceiver.h"
#include "VideoDecoder.h"
#include "VirtualCamera.h"
#include <QDebug>

StreamReceiver::StreamReceiver(uint16_t port, int width, int height, float fps)
    : m_port(port)
    , m_width(width)
    , m_height(height)
    , m_fps(fps)
    , m_acceptor(m_ioc)
{}

StreamReceiver::~StreamReceiver()
{
    stop();
}

bool StreamReceiver::start()
{
    m_decoder = std::make_unique<VideoDecoder>();
    if (!m_decoder->init(0, 0)) return false;

    m_camera = std::make_unique<VirtualCamera>();

    m_decoder->setOnFrame([this](const uint8_t* rgb, int w, int h) {
        if (w != m_width || h != m_height) {
            qDebug() << "StreamReceiver: abrindo câmera" << w << "x" << h;
            m_width  = w;
            m_height = h;
            if (m_camera) m_camera->close();
            m_camera = std::make_unique<VirtualCamera>();
            if (!m_camera->open(w, h, m_fps)) {
                qWarning() << "StreamReceiver: falha ao abrir VirtualCamera";
                return;
            }
        }
        m_camera->sendFrame(rgb, w, h);
    });

    try {
        tcp::endpoint ep(tcp::v4(), m_port);
        m_acceptor.open(ep.protocol());
        m_acceptor.set_option(asio::socket_base::reuse_address(true));
        m_acceptor.bind(ep);
        m_acceptor.listen();
    } catch (const std::exception& e) {
        qWarning() << "StreamReceiver: bind falhou —" << e.what();
        return false;
    }

    m_running = true;
    m_thread  = std::thread(&StreamReceiver::acceptLoop, this);
    return true;
}

void StreamReceiver::stop()
{
    if (!m_running.exchange(false)) return;
    boost::system::error_code ec;
    m_acceptor.close(ec);   // desbloqueia accept() bloqueado
    m_ioc.stop();
    if (m_thread.joinable()) m_thread.join();
    if (m_camera) m_camera->close();
}

void StreamReceiver::acceptLoop()
{
    while (m_running) {
        boost::system::error_code ec;
        tcp::socket socket(m_ioc);
        m_acceptor.accept(socket, ec);
        if (ec) break;

        qDebug() << "StreamReceiver: cliente conectado";
        if (m_onStatus) m_onStatus(true);
        receiveLoop(std::move(socket));
        qDebug() << "StreamReceiver: cliente desconectado";
        if (m_onStatus) m_onStatus(false);
    }
}

void StreamReceiver::receiveLoop(tcp::socket socket)
{
    int frameCount = 0;
    while (m_running) {
        uint8_t lenBuf[4];
        if (!readExact(socket, lenBuf, 4)) break;

        uint32_t naluSize = (uint32_t(lenBuf[0]) << 24) |
                            (uint32_t(lenBuf[1]) << 16) |
                            (uint32_t(lenBuf[2]) <<  8) |
                             uint32_t(lenBuf[3]);

        if (naluSize == 0 || naluSize > 4 * 1024 * 1024) {
            qDebug() << "StreamReceiver: NALU inválido, abortando";
            break;
        }

        std::vector<uint8_t> nalu(naluSize);
        if (!readExact(socket, nalu.data(), naluSize)) break;

        m_decoder->pushNalu(nalu.data(), naluSize);
    }
}
bool StreamReceiver::readExact(tcp::socket& socket, uint8_t* buf, size_t size)
{
    size_t total = 0;
    while (total < size) {
        boost::system::error_code ec;
        size_t n = socket.read_some(asio::buffer(buf + total, size - total), ec);
        if (ec || n == 0) return false;
        total += n;
    }
    return true;
}
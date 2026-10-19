#include "VideoDecoder.h"
#include <QDebug>
#include <stdexcept>
#include <algorithm>

extern "C" {
#include <libavutil/imgutils.h>
}

VideoDecoder::VideoDecoder() = default;

VideoDecoder::~VideoDecoder()
{
    if (m_sws)   sws_freeContext(m_sws);
    if (m_frame) av_frame_free(&m_frame);
    if (m_ctx)   avcodec_free_context(&m_ctx);
}

bool VideoDecoder::init(int targetWidth, int targetHeight)
{
    m_targetWidth  = (targetWidth > 0) ? targetWidth : 1920;
    m_targetHeight = (targetHeight > 0) ? targetHeight : 1080;

    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) return false;

    m_ctx = avcodec_alloc_context3(codec);
    if (!m_ctx) return false;

    m_ctx->flags2 |= AV_CODEC_FLAG2_CHUNKS;

    if (avcodec_open2(m_ctx, codec, nullptr) < 0) return false;

    m_frame = av_frame_alloc();
    if (!m_frame) return false;

    // Buffer RGB (BGR24 para Softcam / DirectShow) alocado para canvas fixo
    m_rgbBuffer.assign(m_targetWidth * m_targetHeight * 3, 0);

    return true;
}

void VideoDecoder::pushNalu(const uint8_t* data, size_t size)
{
    AVPacket* pkt = av_packet_alloc();
    av_new_packet(pkt, static_cast<int>(size));
    memcpy(pkt->data, data, size);
    decode(pkt);
    av_packet_free(&pkt);
}

void VideoDecoder::decode(AVPacket* pkt)
{
    int ret = avcodec_send_packet(m_ctx, pkt);
    if (ret < 0) return;

    while (avcodec_receive_frame(m_ctx, m_frame) == 0) {
        if (m_frame->width != m_srcWidth || m_frame->height != m_srcHeight || m_frame->format != m_lastFormat) {
            m_srcWidth   = m_frame->width;
            m_srcHeight  = m_frame->height;
            m_lastFormat = m_frame->format;

            if (m_srcWidth > 0 && m_srcHeight > 0) {
                double srcAspect    = static_cast<double>(m_srcWidth) / static_cast<double>(m_srcHeight);
                double targetAspect = static_cast<double>(m_targetWidth) / static_cast<double>(m_targetHeight);

                if (srcAspect > targetAspect) {
                    // Mais largo que 16:9 -> letterbox (barras superior e inferior)
                    m_scaledWidth  = m_targetWidth;
                    m_scaledHeight = (static_cast<int>(m_targetWidth / srcAspect)) & ~1;
                    if (m_scaledHeight < 2) m_scaledHeight = 2;
                    m_offsetX      = 0;
                    m_offsetY      = ((m_targetHeight - m_scaledHeight) / 2) & ~1;
                } else {
                    // Mais alto que 16:9 (ex.: tela celular vertical) -> pillarbox (barras laterais)
                    m_scaledHeight = m_targetHeight;
                    m_scaledWidth  = (static_cast<int>(m_targetHeight * srcAspect)) & ~1;
                    if (m_scaledWidth < 2) m_scaledWidth = 2;
                    m_offsetX      = ((m_targetWidth - m_scaledWidth) / 2) & ~1;
                    m_offsetY      = 0;
                }

                if (m_sws) sws_freeContext(m_sws);
                m_sws = sws_getContext(m_srcWidth, m_srcHeight, static_cast<AVPixelFormat>(m_lastFormat),
                                       m_scaledWidth, m_scaledHeight, AV_PIX_FMT_BGR24,
                                       SWS_BILINEAR, nullptr, nullptr, nullptr);

                qDebug() << "VideoDecoder: entrada" << m_srcWidth << "x" << m_srcHeight
                         << "-> escalado" << m_scaledWidth << "x" << m_scaledHeight
                         << "offset (" << m_offsetX << "," << m_offsetY << ") no canvas"
                         << m_targetWidth << "x" << m_targetHeight;

                std::fill(m_rgbBuffer.begin(), m_rgbBuffer.end(), 0);
            }
        }

        convertToRgb(m_frame);
    }
}

void VideoDecoder::convertToRgb(AVFrame* frame)
{
    if (!m_sws || !m_onFrame || m_srcWidth <= 0 || m_srcHeight <= 0) return;

    // Se houver pillarbox ou letterbox, garante que as margens permaneçam pretas limpas
    if (m_scaledWidth < m_targetWidth || m_scaledHeight < m_targetHeight) {
        std::fill(m_rgbBuffer.begin(), m_rgbBuffer.end(), 0);
    }

    uint8_t* dstData[4] = {
        m_rgbBuffer.data() + (m_offsetY * m_targetWidth + m_offsetX) * 3,
        nullptr, nullptr, nullptr
    };
    int dstLinesize[4] = { m_targetWidth * 3, 0, 0, 0 };

    sws_scale(m_sws,
              frame->data, frame->linesize, 0, m_srcHeight,
              dstData, dstLinesize);

    m_onFrame(m_rgbBuffer.data(), m_targetWidth, m_targetHeight);
}
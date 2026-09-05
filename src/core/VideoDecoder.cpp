#include "VideoDecoder.h"
#include <QDebug>
#include <stdexcept>

extern "C" {
#include <libavutil/imgutils.h>
}

VideoDecoder::VideoDecoder() = default;

VideoDecoder::~VideoDecoder()
{
    if (m_sws)      sws_freeContext(m_sws);
    if (m_frameRgb) av_frame_free(&m_frameRgb);
    if (m_frame)    av_frame_free(&m_frame);
    if (m_ctx)      avcodec_free_context(&m_ctx);
}

bool VideoDecoder::init(int width, int height)
{
    m_width  = width;
    m_height = height;

    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) return false;

    m_ctx = avcodec_alloc_context3(codec);
    if (!m_ctx) return false;

    m_ctx->flags2 |= AV_CODEC_FLAG2_CHUNKS;

    if (avcodec_open2(m_ctx, codec, nullptr) < 0) return false;

    m_frame    = av_frame_alloc();
    m_frameRgb = av_frame_alloc();
    if (!m_frame || !m_frameRgb) return false;

    // sws e buffer BGR criados no primeiro frame via decode()
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
        if (m_frame->width != m_width || m_frame->height != m_height) {
            m_width  = m_frame->width;
            m_height = m_frame->height;
            sws_freeContext(m_sws);
            m_sws = sws_getContext(m_width, m_height, static_cast<AVPixelFormat>(m_frame->format),
                                   m_width, m_height, AV_PIX_FMT_BGR24,
                                   SWS_BILINEAR, nullptr, nullptr, nullptr);

            int bufSize = av_image_get_buffer_size(AV_PIX_FMT_BGR24,
                                                   m_width, m_height, 1);
            m_rgbBuffer.resize(bufSize);
            av_image_fill_arrays(m_frameRgb->data, m_frameRgb->linesize,
                                 m_rgbBuffer.data(), AV_PIX_FMT_BGR24,
                                 m_width, m_height, 1);
            m_lastFormat = m_frame->format;
        }
        convertToRgb(m_frame);
    }
}

void VideoDecoder::convertToRgb(AVFrame* frame)
{
    if (!m_sws || !m_onFrame) return;

    if (frame->format != m_lastFormat) {
        sws_freeContext(m_sws);
        m_sws = sws_getContext(m_width, m_height,
                               static_cast<AVPixelFormat>(frame->format),
                               m_width, m_height, AV_PIX_FMT_BGR24,
                               SWS_BILINEAR, nullptr, nullptr, nullptr);
        m_lastFormat = frame->format;
    }

    sws_scale(m_sws,
              frame->data, frame->linesize, 0, m_height,
              m_frameRgb->data, m_frameRgb->linesize);
    m_onFrame(m_rgbBuffer.data(), m_width, m_height);
}
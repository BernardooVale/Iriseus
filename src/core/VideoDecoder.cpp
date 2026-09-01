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
    if (!codec) {
        qWarning() << "VideoDecoder: H264 decoder não encontrado";
        return false;
    }

    m_ctx = avcodec_alloc_context3(codec);
    if (!m_ctx) return false;

    // Permite receber NALUs sem container (stream raw)
    m_ctx->flags2 |= AV_CODEC_FLAG2_CHUNKS;

    if (avcodec_open2(m_ctx, codec, nullptr) < 0) {
        qWarning() << "VideoDecoder: falha ao abrir codec";
        return false;
    }

    m_frame    = av_frame_alloc();
    m_frameRgb = av_frame_alloc();
    if (!m_frame || !m_frameRgb) return false;

    // Buffer RGB24 para scSendFrame()
    int bufSize = av_image_get_buffer_size(AV_PIX_FMT_RGB24, width, height, 1);
    m_rgbBuffer.resize(bufSize);
    av_image_fill_arrays(m_frameRgb->data, m_frameRgb->linesize,
                         m_rgbBuffer.data(), AV_PIX_FMT_RGB24, width, height, 1);

    m_sws = sws_getContext(width, height, AV_PIX_FMT_YUV420P,
                           width, height, AV_PIX_FMT_RGB24,
                           SWS_BILINEAR, nullptr, nullptr, nullptr);
    return m_sws != nullptr;
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
        // Reinicia sws se resolução mudou (ex: primeira configuração do codec)
        if (m_frame->width != m_width || m_frame->height != m_height) {
            m_width  = m_frame->width;
            m_height = m_frame->height;
            sws_freeContext(m_sws);
            m_sws = sws_getContext(m_width, m_height, AV_PIX_FMT_YUV420P,
                                   m_width, m_height, AV_PIX_FMT_RGB24,
                                   SWS_BILINEAR, nullptr, nullptr, nullptr);

            int bufSize = av_image_get_buffer_size(AV_PIX_FMT_RGB24,
                                                   m_width, m_height, 1);
            m_rgbBuffer.resize(bufSize);
            av_image_fill_arrays(m_frameRgb->data, m_frameRgb->linesize,
                                 m_rgbBuffer.data(), AV_PIX_FMT_RGB24,
                                 m_width, m_height, 1);
        }
        convertToRgb(m_frame);
    }
}

void VideoDecoder::convertToRgb(AVFrame* frame)
{
    if (!m_sws || !m_onFrame) return;
    sws_scale(m_sws,
              frame->data, frame->linesize, 0, m_height,
              m_frameRgb->data, m_frameRgb->linesize);
    m_onFrame(m_rgbBuffer.data(), m_width, m_height);
}
#pragma once
#include <functional>
#include <vector>
#include <cstdint>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
}

// Decodifica stream H264 → frames RGB24 prontos para scSendFrame()

class VideoDecoder
{
public:
    // callback: buffer RGB24, width, height
    using OnFrame = std::function<void(const uint8_t* rgb, int width, int height)>;

    VideoDecoder();
    ~VideoDecoder();

    bool init(int targetWidth = 1920, int targetHeight = 1080);
    void pushNalu(const uint8_t* data, size_t size); // recebe NALUs do socket
    void setOnFrame(OnFrame cb) { m_onFrame = std::move(cb); }

    int targetWidth() const { return m_targetWidth; }
    int targetHeight() const { return m_targetHeight; }

private:
    void decode(AVPacket* pkt);
    void convertToRgb(AVFrame* frame);

    AVCodecContext* m_ctx      = nullptr;
    AVFrame*        m_frame    = nullptr;
    SwsContext*     m_sws      = nullptr;

    int m_targetWidth  = 1920;
    int m_targetHeight = 1080;

    int m_srcWidth     = 0;
    int m_srcHeight    = 0;
    int m_scaledWidth  = 0;
    int m_scaledHeight = 0;
    int m_offsetX      = 0;
    int m_offsetY      = 0;
    int m_lastFormat   = -1;

    std::vector<uint8_t> m_rgbBuffer;
    OnFrame              m_onFrame;
};
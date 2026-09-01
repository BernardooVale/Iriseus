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

    bool init(int width, int height);
    void pushNalu(const uint8_t* data, size_t size); // recebe NALUs do socket
    void setOnFrame(OnFrame cb) { m_onFrame = std::move(cb); }

private:
    void decode(AVPacket* pkt);
    void convertToRgb(AVFrame* frame);

    AVCodecContext* m_ctx      = nullptr;
    AVFrame*        m_frame    = nullptr;
    AVFrame*        m_frameRgb = nullptr;
    SwsContext*     m_sws      = nullptr;

    int m_width  = 0;
    int m_height = 0;

    std::vector<uint8_t> m_rgbBuffer;
    OnFrame              m_onFrame;
};
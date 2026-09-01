#pragma once
#include <cstdint>
#include <windows.h>

// Carrega softcam.dll em runtime via LoadLibrary
// Evita linkar DShowSoftcam e baseclasses no executável principal

using scCamera = void*;
using FnScCreateCamera = scCamera(*)(int, int, float);
using FnScDeleteCamera = void(*)(scCamera);
using FnScSendFrame    = void(*)(scCamera, const void*);

class VirtualCamera
{
public:
    VirtualCamera() = default;
    ~VirtualCamera();

    bool open(int width, int height, float fps);
    void sendFrame(const uint8_t* rgb, int width, int height);
    void close();

    bool isOpen() const { return m_cam != nullptr; }

private:
    bool loadDll();

    HMODULE            m_dll    = nullptr;
    scCamera           m_cam    = nullptr;
    FnScCreateCamera   m_fnCreate = nullptr;
    FnScDeleteCamera   m_fnDelete = nullptr;
    FnScSendFrame      m_fnSend   = nullptr;
    int                m_width  = 0;
    int                m_height = 0;
};
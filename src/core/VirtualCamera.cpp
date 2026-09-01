#include "VirtualCamera.h"
#include <QDebug>
#include <QCoreApplication>

VirtualCamera::~VirtualCamera()
{
    close();
}

bool VirtualCamera::loadDll()
{
    if (m_dll) return true;

    // Procura softcam.dll na pasta do executável
    QString exeDir = QCoreApplication::applicationDirPath();
    QString dllPath = exeDir + "/softcam.dll";

    m_dll = LoadLibraryW(dllPath.toStdWString().c_str());
    if (!m_dll) {
        qWarning() << "VirtualCamera: softcam.dll não encontrada em" << dllPath;
        return false;
    }

    m_fnCreate = (FnScCreateCamera)GetProcAddress(m_dll, "scCreateCamera");
    m_fnDelete = (FnScDeleteCamera)GetProcAddress(m_dll, "scDeleteCamera");
    m_fnSend   = (FnScSendFrame)   GetProcAddress(m_dll, "scSendFrame");

    if (!m_fnCreate || !m_fnDelete || !m_fnSend) {
        qWarning() << "VirtualCamera: símbolos não encontrados em softcam.dll";
        FreeLibrary(m_dll);
        m_dll = nullptr;
        return false;
    }
    return true;
}

bool VirtualCamera::open(int width, int height, float fps)
{
    if (!loadDll()) return false;

    m_cam = m_fnCreate(width, height, fps);
    if (!m_cam) {
        qWarning() << "VirtualCamera: scCreateCamera falhou — Softcam registrado?";
        return false;
    }
    m_width  = width;
    m_height = height;
    qDebug() << "VirtualCamera: aberta" << width << "x" << height << "@" << fps;
    return true;
}

void VirtualCamera::sendFrame(const uint8_t* rgb, int width, int height)
{
    if (!m_cam || !m_fnSend) return;
    m_fnSend(m_cam, rgb);
}

void VirtualCamera::close()
{
    if (m_cam && m_fnDelete) {
        m_fnDelete(m_cam);
        m_cam = nullptr;
    }
    if (m_dll) {
        FreeLibrary(m_dll);
        m_dll = nullptr;
    }
}
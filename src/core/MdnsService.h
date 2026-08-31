#pragma once
#include <string>
#include <thread>
#include <atomic>

// Anuncia "_iriseus._tcp.local" via mDNS para descoberta pelo app Android

class MdnsService
{
public:
    MdnsService(std::string deviceName, uint16_t wsPort);
    ~MdnsService();

    bool start();
    void stop();

private:
    void runLoop();

    std::string       m_deviceName;
    uint16_t          m_wsPort;
    std::thread       m_thread;
    std::atomic<bool> m_running{false};
};
#define _WIN32_WINNT 0x0601
#define MDNS_IMPLEMENTATION
#include "mdns.h"
#include "MdnsService.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <array>
#include <string>
#include <chrono>
#include <thread>

#pragma comment(lib, "iphlpapi.lib")

static constexpr const char* kServiceType = "_iriseus._tcp.local.";

// Obtém primeiro IP IPv4 não-loopback da máquina
static uint32_t getLocalIp()
{
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    if (getaddrinfo(hostname, nullptr, &hints, &res) != 0) return 0;
    uint32_t ip = 0;
    for (auto* p = res; p; p = p->ai_next) {
        auto* s = reinterpret_cast<struct sockaddr_in*>(p->ai_addr);
        uint32_t candidate = s->sin_addr.s_addr;
        // pular loopback
        if ((ntohl(candidate) >> 24) == 127) continue;
        ip = candidate;
        break;
    }
    freeaddrinfo(res);
    return ip;
}

struct ServiceContext {
    std::string serviceInst;
    std::string hostname;
    std::string txtRecord;
    uint16_t    port;
    uint32_t    localIp;
};

static int mdnsCallback(int sock, const struct sockaddr* from, size_t addrlen,
                        mdns_entry_type_t entry, uint16_t query_id,
                        uint16_t rtype, uint16_t rclass, uint32_t ttl,
                        const void* data, size_t size, size_t name_offset,
                        size_t name_length, size_t record_offset,
                        size_t record_length, void* user_data)
{
    // Só responde queries (não announcements ou answers de outros)
    if (entry != MDNS_ENTRYTYPE_QUESTION) return 0;

    auto* ctx = static_cast<ServiceContext*>(user_data);
    std::array<char, 2048> buffer;

    mdns_record_t answer{};
    mdns_record_t additional[3];
    int nadditional = 0;

    if (rtype == MDNS_RECORDTYPE_PTR) {
        // Responde PTR com SRV + TXT + A como additional
        answer.name              = {kServiceType, strlen(kServiceType)};
        answer.type              = MDNS_RECORDTYPE_PTR;
        answer.data.ptr.name     = {ctx->serviceInst.c_str(), ctx->serviceInst.size()};
        answer.ttl               = 120;

        additional[0].name             = {ctx->serviceInst.c_str(), ctx->serviceInst.size()};
        additional[0].type             = MDNS_RECORDTYPE_SRV;
        additional[0].data.srv.name    = {ctx->hostname.c_str(), ctx->hostname.size()};
        additional[0].data.srv.port    = ctx->port;
        additional[0].data.srv.priority = 0;
        additional[0].data.srv.weight  = 0;
        additional[0].ttl              = 120;

        additional[1].name             = {ctx->serviceInst.c_str(), ctx->serviceInst.size()};
        additional[1].type             = MDNS_RECORDTYPE_TXT;
        additional[1].data.txt.key     = {"version", 7};
        additional[1].data.txt.value   = {"1", 1};
        additional[1].ttl              = 120;

        additional[2].name                        = {ctx->hostname.c_str(), ctx->hostname.size()};
        additional[2].type                        = MDNS_RECORDTYPE_A;
        additional[2].data.a.addr.sin_family      = AF_INET;
        additional[2].data.a.addr.sin_addr.s_addr = ctx->localIp; // IP real
        additional[2].ttl                         = 120;

        nadditional = 3;

        mdns_query_answer_multicast(sock, buffer.data(), buffer.size(),
                                    answer, nullptr, 0,
                                    additional, nadditional);
    }
    else if (rtype == MDNS_RECORDTYPE_SRV) {
        answer.name             = {ctx->serviceInst.c_str(), ctx->serviceInst.size()};
        answer.type             = MDNS_RECORDTYPE_SRV;
        answer.data.srv.name    = {ctx->hostname.c_str(), ctx->hostname.size()};
        answer.data.srv.port    = ctx->port;
        answer.data.srv.priority = 0;
        answer.data.srv.weight  = 0;
        answer.ttl              = 120;

        mdns_query_answer_multicast(sock, buffer.data(), buffer.size(),
                                    answer, nullptr, 0, nullptr, 0);
    }
    else if (rtype == MDNS_RECORDTYPE_A) {
        answer.name                        = {ctx->hostname.c_str(), ctx->hostname.size()};
        answer.type                        = MDNS_RECORDTYPE_A;
        answer.data.a.addr.sin_family      = AF_INET;
        answer.data.a.addr.sin_addr.s_addr = ctx->localIp;
        answer.ttl                         = 120;

        mdns_query_answer_multicast(sock, buffer.data(), buffer.size(),
                                    answer, nullptr, 0, nullptr, 0);
    }

    return 0;
}

MdnsService::MdnsService(std::string deviceName, uint16_t wsPort)
    : m_deviceName(std::move(deviceName))
    , m_wsPort(wsPort)
{}

MdnsService::~MdnsService() { stop(); }

bool MdnsService::start()
{
    m_running = true;
    m_thread  = std::thread(&MdnsService::runLoop, this);
    return true;
}

void MdnsService::stop()
{
    if (!m_running.exchange(false)) return;
    if (m_thread.joinable()) m_thread.join();
}

void MdnsService::runLoop()
{
    uint32_t localIp = getLocalIp();
    if (localIp == 0) return;

    struct sockaddr_in saddr{};
    saddr.sin_family      = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_port        = htons(MDNS_PORT);

    int sock = mdns_socket_open_ipv4(&saddr);
    if (sock < 0) return;

    std::string hostname    = m_deviceName + ".local.";
    std::string serviceInst = m_deviceName + "." + kServiceType;

    ServiceContext ctx{serviceInst, hostname, "version=1", m_wsPort, localIp};

    std::array<char, 2048> buffer;

    // Monta records com IP real
    mdns_record_t ptr{};
    ptr.name             = {kServiceType, strlen(kServiceType)};
    ptr.type             = MDNS_RECORDTYPE_PTR;
    ptr.data.ptr.name    = {serviceInst.c_str(), serviceInst.size()};
    ptr.ttl              = 120;

    mdns_record_t additional[3];

    additional[0].name             = {serviceInst.c_str(), serviceInst.size()};
    additional[0].type             = MDNS_RECORDTYPE_SRV;
    additional[0].data.srv.name    = {hostname.c_str(), hostname.size()};
    additional[0].data.srv.port    = m_wsPort;
    additional[0].data.srv.priority = 0;
    additional[0].data.srv.weight  = 0;
    additional[0].ttl              = 120;

    additional[1].name             = {serviceInst.c_str(), serviceInst.size()};
    additional[1].type             = MDNS_RECORDTYPE_TXT;
    additional[1].data.txt.key     = {"version", 7};
    additional[1].data.txt.value   = {"1", 1};
    additional[1].ttl              = 120;

    additional[2].name                        = {hostname.c_str(), hostname.size()};
    additional[2].type                        = MDNS_RECORDTYPE_A;
    additional[2].data.a.addr.sin_family      = AF_INET;
    additional[2].data.a.addr.sin_addr.s_addr = localIp; // IP real
    additional[2].ttl                         = 120;

    // Announce inicial
    mdns_announce_multicast(sock, buffer.data(), buffer.size(),
                            ptr, nullptr, 0, additional, 3);

    // Loop: escuta e responde queries
    while (m_running) {
        struct timeval tv{0, 500000};
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET((SOCKET)sock, &readfds);

        int ready = select(sock + 1, &readfds, nullptr, nullptr, &tv);
        if (ready <= 0) continue;

        mdns_socket_listen(sock, buffer.data(), buffer.size(), mdnsCallback, &ctx);
    }

    // Goodbye
    ptr.ttl          = 0;
    additional[0].ttl = 0;
    additional[1].ttl = 0;
    additional[2].ttl = 0;
    mdns_goodbye_multicast(sock, buffer.data(), buffer.size(),
                           ptr, nullptr, 0, additional, 3);

    mdns_socket_close(sock);
}
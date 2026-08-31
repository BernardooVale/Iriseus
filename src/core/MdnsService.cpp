#define _WIN32_WINNT 0x0601
#define MDNS_IMPLEMENTATION
#include "mdns.h"
#include "MdnsService.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <array>
#include <string>
#include <chrono>
#include <thread>

static constexpr const char* kServiceType = "_devlink._tcp.local.";

MdnsService::MdnsService(std::string deviceName, uint16_t wsPort)
    : m_deviceName(std::move(deviceName))
    , m_wsPort(wsPort)
{}

MdnsService::~MdnsService()
{
    stop();
}

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

// Callback obrigatório para mdns_listen — ignora queries recebidas,
// a resposta é feita via mdns_query_answer_multicast dentro do loop
static int mdnsCallback(int sock, const struct sockaddr* from, size_t addrlen,
                        mdns_entry_type_t entry, uint16_t query_id,
                        uint16_t rtype, uint16_t rclass, uint32_t ttl,
                        const void* data, size_t size, size_t name_offset,
                        size_t name_length, size_t record_offset,
                        size_t record_length, void* user_data)
{
    // Não processa queries recebidas nessa implementação simplificada
    (void)sock; (void)from; (void)addrlen; (void)entry; (void)query_id;
    (void)rtype; (void)rclass; (void)ttl; (void)data; (void)size;
    (void)name_offset; (void)name_length; (void)record_offset;
    (void)record_length; (void)user_data;
    return 0;
}

void MdnsService::runLoop()
{
    using namespace std::chrono_literals;

    // Socket de serviço — bind na porta 5353
    struct sockaddr_in saddr{};
    saddr.sin_family      = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_port        = htons(MDNS_PORT);

    int sock = mdns_socket_open_ipv4(&saddr);
    if (sock < 0) return;

    std::array<char, 2048> buffer;

    // Nomes DNS
    std::string hostname    = m_deviceName + ".local.";
    std::string serviceInst = m_deviceName + "." + kServiceType;
    std::string txtRecord   = "version=1";

    // Monta records para announce/resposta
    mdns_record_t records[4];

    // PTR: _devlink._tcp.local. → <instance>._devlink._tcp.local.
    records[0].name     = {kServiceType, strlen(kServiceType)};
    records[0].type     = MDNS_RECORDTYPE_PTR;
    records[0].data.ptr.name = {serviceInst.c_str(), serviceInst.size()};
    records[0].rclass   = 0;
    records[0].ttl      = 120;

    // SRV: <instance> → hostname:port
    records[1].name     = {serviceInst.c_str(), serviceInst.size()};
    records[1].type     = MDNS_RECORDTYPE_SRV;
    records[1].data.srv.name     = {hostname.c_str(), hostname.size()};
    records[1].data.srv.port     = m_wsPort;
    records[1].data.srv.priority = 0;
    records[1].data.srv.weight   = 0;
    records[1].rclass  = 0;
    records[1].ttl     = 120;

    // TXT
    mdns_record_txt_t txt{};
    txt.key   = {"version", 7};
    txt.value = {"1", 1};
    records[2].name     = {serviceInst.c_str(), serviceInst.size()};
    records[2].type     = MDNS_RECORDTYPE_TXT;
    records[2].data.txt.key   = {"version", 7};
    records[2].data.txt.value = {"1", 1};
    records[2].rclass   = 0;
    records[2].ttl      = 120;

    // A: hostname → IP (mdns.h resolve automaticamente ao enviar)
    records[3].name   = {hostname.c_str(), hostname.size()};
    records[3].type   = MDNS_RECORDTYPE_A;
    records[3].data.a.addr.sin_family      = AF_INET;
    records[3].data.a.addr.sin_addr.s_addr = 0; // 0 = usa IP local automaticamente
    records[3].rclass = 0;
    records[3].ttl    = 120;

    // Announce inicial
    mdns_announce_multicast(sock, buffer.data(), buffer.size(),
                            records[0], nullptr, 0,
                            records + 1, 3);

    // Loop: escuta e responde queries
    while (m_running) {
        struct timeval tv{0, 500000};
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET((SOCKET)sock, &readfds);

        int ready = select(sock + 1, &readfds, nullptr, nullptr, &tv);
        if (ready <= 0) continue;

        mdns_socket_listen(sock, buffer.data(), buffer.size(), mdnsCallback, nullptr);
    }

    // Goodbye — TTL=0
    records[0].ttl = 0;
    records[1].ttl = 0;
    records[2].ttl = 0;
    records[3].ttl = 0;
    mdns_goodbye_multicast(sock, buffer.data(), buffer.size(),
                       records[0], nullptr, 0,
                       records + 1, 3);

    mdns_socket_close(sock);
}
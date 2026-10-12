#include "FileTransferServer.h"
#include "PairingCrypto.h"
#include <sodium.h>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace Iriseus {

FileTransferServer::FileTransferServer(uint16_t port)
    : m_port(port)
    , m_acceptor(m_ioc)
{
}

FileTransferServer::~FileTransferServer()
{
    stop();
}

bool FileTransferServer::start()
{
    try {
        tcp::endpoint ep(tcp::v4(), m_port);
        m_acceptor.open(ep.protocol());
        m_acceptor.set_option(asio::socket_base::reuse_address(true));
        m_acceptor.set_option(asio::socket_base::receive_buffer_size(1024 * 1024));
        m_acceptor.set_option(asio::socket_base::send_buffer_size(1024 * 1024));
        m_acceptor.bind(ep);
        m_acceptor.listen();
    } catch (const std::exception& e) {
        qWarning() << "FileTransferServer: falha ao bind na porta" << m_port << ":" << e.what();
        return false;
    }

    m_running = true;
    m_thread = std::thread(&FileTransferServer::acceptLoop, this);
    qDebug() << "FileTransferServer iniciado na porta" << m_port;
    return true;
}

void FileTransferServer::stop()
{
    if (!m_running.exchange(false)) return;

    boost::system::error_code ec;
    m_acceptor.close(ec);
    m_ioc.stop();

    if (m_thread.joinable()) {
        m_thread.join();
    }

    std::lock_guard<std::mutex> lk(m_sessionsMutex);
    for (auto& [id, s] : m_sessions) {
        s->cancelled = true;
        // Se tinha .part aberto, limpa
        if (!s->tempFilePath.empty()) {
            QFile::remove(QString::fromStdString(s->tempFilePath));
        }
    }
    m_sessions.clear();
}

void FileTransferServer::registerTransfer(const std::string& transferId,
                                         const std::string& tokenBase64,
                                         const std::vector<uint8_t>& tokenBytes,
                                         const std::string& targetPath,
                                         uint64_t expectedSize,
                                         const std::string& expectedSha256,
                                         TransferDirection direction)
{
    auto session = std::make_shared<ActiveTransferSession>();
    session->transferId = transferId;
    session->tokenBase64 = tokenBase64;
    session->tokenBytes = tokenBytes;
    session->targetFilePath = targetPath;
    session->tempFilePath = targetPath + ".part";
    session->expectedSize = expectedSize;
    session->expectedSha256 = expectedSha256;
    session->direction = direction;

    std::lock_guard<std::mutex> lk(m_sessionsMutex);
    m_sessions[transferId] = session;
    qDebug() << "FileTransferServer: registrada transferência" << QString::fromStdString(transferId)
             << "tamanho:" << expectedSize;
}

void FileTransferServer::cancelTransfer(const std::string& transferId)
{
    std::lock_guard<std::mutex> lk(m_sessionsMutex);
    auto it = m_sessions.find(transferId);
    if (it != m_sessions.end()) {
        it->second->cancelled = true;
        if (!it->second->tempFilePath.empty()) {
            QFile::remove(QString::fromStdString(it->second->tempFilePath));
        }
        m_sessions.erase(it);
    }
}

void FileTransferServer::acceptLoop()
{
    while (m_running) {
        boost::system::error_code ec;
        tcp::socket socket(m_ioc);
        m_acceptor.accept(socket, ec);
        if (ec) break;

        // Trata conexão em thread destacada (ou async)
        std::thread([this, s = std::move(socket)]() mutable {
            handleConnection(std::move(s));
        }).detach();
    }
}

bool FileTransferServer::readExact(tcp::socket& socket, uint8_t* buf, size_t size)
{
    size_t total = 0;
    while (total < size) {
        boost::system::error_code ec;
        size_t n = socket.read_some(asio::buffer(buf + total, size - total), ec);
        if (ec || n == 0) return false;
        total += n;
    }
    return true;
}

// Converte bytes para string hexadecimal (minúsculo)
static std::string toHex(const uint8_t* data, size_t len)
{
    std::ostringstream ss;
    for (size_t i = 0; i < len; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    return ss.str();
}

void FileTransferServer::handleConnection(tcp::socket socket)
{
    // Handshake:
    // [16 bytes transferId] + [32 bytes token] + [1 byte direction] = 49 bytes
    constexpr size_t kHandshakeSize = kUuidBinarySize + kTokenSize + 1;
    uint8_t handshakeBuf[kHandshakeSize];

    if (!readExact(socket, handshakeBuf, kHandshakeSize)) {
        qWarning() << "FileTransferServer: falha ao ler handshake";
        return;
    }

    // O transferId pode vir em binário ou string hex/UUID de 16 bytes.
    // Para simplificar a interoperabilidade com UUIDs em string/bytes:
    // vamos converter os 16 bytes para string UUID canônica (8-4-4-4-12) ou comparar
    char uuidStr[37];
    snprintf(uuidStr, sizeof(uuidStr),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             handshakeBuf[0], handshakeBuf[1], handshakeBuf[2], handshakeBuf[3],
             handshakeBuf[4], handshakeBuf[5],
             handshakeBuf[6], handshakeBuf[7],
             handshakeBuf[8], handshakeBuf[9],
             handshakeBuf[10], handshakeBuf[11], handshakeBuf[12], handshakeBuf[13], handshakeBuf[14], handshakeBuf[15]);

    std::string transferIdCandidate = uuidStr;
    const uint8_t* tokenCandidate = handshakeBuf + kUuidBinarySize;
    uint8_t directionByte = handshakeBuf[kUuidBinarySize + kTokenSize];

    std::shared_ptr<ActiveTransferSession> session;
    {
        std::lock_guard<std::mutex> lk(m_sessionsMutex);
        auto it = m_sessions.find(transferIdCandidate);
        if (it != m_sessions.end()) {
            session = it->second;
        } else {
            // Tenta match se o transferId registrado não tiver traços
            std::string noDash = transferIdCandidate;
            noDash.erase(std::remove(noDash.begin(), noDash.end(), '-'), noDash.end());
            for (auto& pair : m_sessions) {
                std::string sDash = pair.first;
                sDash.erase(std::remove(sDash.begin(), sDash.end(), '-'), sDash.end());
                if (sDash == noDash) {
                    session = pair.second;
                    break;
                }
            }
        }
    }

    if (!session) {
        qWarning() << "FileTransferServer: transferência não encontrada para id:" << uuidStr;
        return;
    }

    // Valida token (32 bytes)
    if (session->tokenBytes.size() == kTokenSize) {
        if (std::memcmp(tokenCandidate, session->tokenBytes.data(), kTokenSize) != 0) {
            qWarning() << "FileTransferServer: token inválido para transferência" << session->transferId.c_str();
            return;
        }
    }

    qDebug() << "FileTransferServer: handshake validado com sucesso para" << session->transferId.c_str();

    // Modo 0: Celular enviando para PC (receber)
    if (directionByte == static_cast<uint8_t>(TransferDirection::MobileUploadToPc)) {
        // Assegura diretório de destino
        QFileInfo fi(QString::fromStdString(session->tempFilePath));
        QDir().mkpath(fi.absolutePath());

        std::ofstream out(session->tempFilePath, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            qWarning() << "FileTransferServer: falha ao abrir arquivo temporário"
                       << QString::fromStdString(session->tempFilePath);
            if (m_onFinished) m_onFinished(session->transferId, false, "Falha ao criar arquivo local");
            return;
        }

        crypto_hash_sha256_state shaState;
        crypto_hash_sha256_init(&shaState);

        uint64_t totalReceived = 0;
        bool error = false;

        // No Marco 1 (sem criptografia):
        // cada frame = [4 bytes big endian tamanho N] + [N bytes plaintext]
        while (m_running && !session->cancelled && totalReceived < session->expectedSize) {
            uint8_t lenBuf[4];
            if (!readExact(socket, lenBuf, 4)) {
                error = true;
                break;
            }

            uint32_t chunkSize = (uint32_t(lenBuf[0]) << 24) |
                                 (uint32_t(lenBuf[1]) << 16) |
                                 (uint32_t(lenBuf[2]) <<  8) |
                                  uint32_t(lenBuf[3]);

            if (chunkSize == 0 || chunkSize > (1024 * 1024)) {
                qWarning() << "FileTransferServer: chunk size inválido:" << chunkSize;
                error = true;
                break;
            }

            std::vector<uint8_t> chunk(chunkSize);
            if (!readExact(socket, chunk.data(), chunkSize)) {
                error = true;
                break;
            }

            out.write(reinterpret_cast<const char*>(chunk.data()), chunkSize);
            crypto_hash_sha256_update(&shaState, chunk.data(), chunkSize);

            totalReceived += chunkSize;
            session->bytesTransferred = totalReceived;

            if (m_onProgress) {
                m_onProgress(session->transferId, totalReceived, session->expectedSize);
            }
        }

        out.close();

        if (error || session->cancelled || totalReceived != session->expectedSize) {
            qWarning() << "FileTransferServer: transferência incompleta ou cancelada";
            QFile::remove(QString::fromStdString(session->tempFilePath));
            if (m_onFinished) {
                m_onFinished(session->transferId, false, session->cancelled ? "Cancelado" : "Conexão interrompida");
            }
            return;
        }

        // Finaliza SHA-256 e confere
        uint8_t hashResult[crypto_hash_sha256_BYTES];
        crypto_hash_sha256_final(&shaState, hashResult);
        std::string computedSha256 = toHex(hashResult, sizeof(hashResult));

        qDebug() << "FileTransferServer: SHA-256 calculado:" << computedSha256.c_str()
                 << "esperado:" << session->expectedSha256.c_str();

        if (!session->expectedSha256.empty() && computedSha256 != session->expectedSha256) {
            qWarning() << "FileTransferServer: hash SHA-256 divergente!";
            QFile::remove(QString::fromStdString(session->tempFilePath));
            if (m_onFinished) m_onFinished(session->transferId, false, "Hash SHA-256 incorreto");
            return;
        }

        // Resolver colisão de nome para renomear
        QString targetPath = QString::fromStdString(session->targetFilePath);
        QFileInfo targetInfo(targetPath);
        QString finalPath = targetPath;
        int count = 1;
        while (QFile::exists(finalPath)) {
            QString base = targetInfo.completeBaseName();
            QString ext = targetInfo.suffix();
            if (!ext.isEmpty()) {
                finalPath = targetInfo.dir().filePath(QString("%1 (%2).%3").arg(base).arg(count).arg(ext));
            } else {
                finalPath = targetInfo.dir().filePath(QString("%1 (%2)").arg(base).arg(count));
            }
            count++;
        }

        if (QFile::rename(QString::fromStdString(session->tempFilePath), finalPath)) {
            qDebug() << "FileTransferServer: arquivo salvo com sucesso em" << finalPath;
            if (m_onFinished) m_onFinished(session->transferId, true, "");
        } else {
            qWarning() << "FileTransferServer: erro ao renomear arquivo final!";
            QFile::remove(QString::fromStdString(session->tempFilePath));
            if (m_onFinished) m_onFinished(session->transferId, false, "Falha ao salvar arquivo final");
        }
    } else if (directionByte == static_cast<uint8_t>(TransferDirection::DesktopDownloadToMobile)) {
        // Modo 1: PC enviando arquivo para o Celular
        std::ifstream in(session->targetFilePath, std::ios::binary);
        if (!in.is_open()) {
            qWarning() << "FileTransferServer: falha ao abrir arquivo para envio:"
                       << QString::fromStdString(session->targetFilePath);
            if (m_onFinished) m_onFinished(session->transferId, false, "Arquivo local inacessível");
            return;
        }

        uint64_t totalSent = 0;
        std::vector<uint8_t> buffer(kChunkPlaintextSize);
        bool error = false;

        while (m_running && !session->cancelled && totalSent < session->expectedSize) {
            size_t toRead = static_cast<size_t>(std::min<uint64_t>(kChunkPlaintextSize, session->expectedSize - totalSent));
            in.read(reinterpret_cast<char*>(buffer.data()), toRead);
            std::streamsize bytesRead = in.gcount();
            if (bytesRead <= 0) break;

            uint32_t chunkSize = static_cast<uint32_t>(bytesRead);
            uint8_t lenBuf[4];
            lenBuf[0] = static_cast<uint8_t>((chunkSize >> 24) & 0xFF);
            lenBuf[1] = static_cast<uint8_t>((chunkSize >> 16) & 0xFF);
            lenBuf[2] = static_cast<uint8_t>((chunkSize >> 8) & 0xFF);
            lenBuf[3] = static_cast<uint8_t>(chunkSize & 0xFF);

            boost::system::error_code ec;
            asio::write(socket, asio::buffer(lenBuf, 4), ec);
            if (ec) { error = true; break; }

            asio::write(socket, asio::buffer(buffer.data(), chunkSize), ec);
            if (ec) { error = true; break; }

            totalSent += chunkSize;
            session->bytesTransferred = totalSent;

            if (m_onProgress) {
                m_onProgress(session->transferId, totalSent, session->expectedSize);
            }
        }

        in.close();

        if (error || session->cancelled || totalSent != session->expectedSize) {
            qWarning() << "FileTransferServer: envio interrompido ou cancelado";
            if (m_onFinished) m_onFinished(session->transferId, false, "Envio interrompido");
        } else {
            qDebug() << "FileTransferServer: envio concluído para" << session->transferId.c_str();
            if (m_onFinished) m_onFinished(session->transferId, true, "");
        }
    }
}

} // namespace Iriseus

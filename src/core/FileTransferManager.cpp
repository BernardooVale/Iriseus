#include "FileTransferManager.h"
#include "WsServer.h"
#include "PairingManager.h"
#include "PairingCrypto.h"
#include <sodium.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDebug>

namespace Iriseus {

FileTransferManager::FileTransferManager(WsServer* wsServer, PairingManager* pairingManager, QObject* parent)
    : QObject(parent)
    , m_wsServer(wsServer)
    , m_pairingManager(pairingManager)
    , m_server(std::make_unique<FileTransferServer>(kFileTransferPort))
{
    m_server->setOnProgress([this](const std::string& transferId, uint64_t bytes, uint64_t total) {
        emit transferProgress(QString::fromStdString(transferId), bytes, total);
    });

    m_server->setOnFinished([this](const std::string& transferId, bool success, const std::string& error) {
        uint64_t sessionId = 0;
        std::string fileName = "";

        auto it = m_activeOffers.find(transferId);
        if (it != m_activeOffers.end()) {
            sessionId = it->second.sessionId;
            fileName = it->second.offer.name;
        }

        // Responde file_complete ao remetente via WebSocket
        if (sessionId != 0 && m_wsServer) {
            nlohmann::json reply = {
                {"type", "file_complete"},
                {"transferId", transferId},
                {"ok", success}
            };
            if (!success && !error.empty()) {
                reply["error"] = error;
            }
            m_wsServer->sendTo(sessionId, reply.dump());
        }

        emit transferFinished(QString::fromStdString(transferId),
                              QString::fromStdString(fileName),
                              success,
                              QString::fromStdString(error));

        m_activeOffers.erase(transferId);
    });
}

FileTransferManager::~FileTransferManager()
{
    stop();
}

bool FileTransferManager::start()
{
    return m_server->start();
}

void FileTransferManager::stop()
{
    m_server->stop();
}

QString FileTransferManager::defaultDownloadDir() const
{
    QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (downloads.isEmpty()) {
        downloads = QDir::homePath() + "/Downloads";
    }
    return downloads + "/Iriseus";
}

QString FileTransferManager::sanitizeFileName(const QString& name)
{
    QString clean = QFileInfo(name).fileName(); // descarta qualquer caminho
    clean.remove(QRegularExpression("[\\\\/:*?\"<>|]"));
    if (clean.isEmpty()) {
        clean = "arquivo_recebido";
    }
    return clean;
}

std::string FileTransferManager::generateToken(std::vector<uint8_t>& tokenBytes)
{
    tokenBytes.resize(kTokenSize);
    randombytes_buf(tokenBytes.data(), tokenBytes.size());
    return PairingCrypto::toBase64(tokenBytes.data(), tokenBytes.size());
}

void FileTransferManager::handleFileOffer(uint64_t sessionId, const nlohmann::json& payload)
{
    try {
        FileOffer offer = payload.get<FileOffer>();

        qDebug() << "FileTransferManager: oferta de arquivo recebida:"
                 << QString::fromStdString(offer.name)
                 << "tamanho:" << offer.size
                 << "de sessionId:" << sessionId;

        // Decisão de aceite automático: dispositivo pareado é aceito automaticamente
        // (Aqui, como já passou da autenticação ou se houver dispositivo conectado)
        // Envia file_accept
        nlohmann::json acceptMsg = {
            {"type", "file_accept"},
            {"transferId", offer.transferId}
        };
        m_wsServer->sendTo(sessionId, acceptMsg.dump());

        // Prepara recepção
        std::vector<uint8_t> tokenBytes;
        std::string tokenBase64 = generateToken(tokenBytes);

        QString downloadFolder = defaultDownloadDir();
        QDir().mkpath(downloadFolder);
        QString cleanName = sanitizeFileName(QString::fromStdString(offer.name));
        QString fullPath = downloadFolder + "/" + cleanName;

        m_activeOffers[offer.transferId] = TransferMeta{ sessionId, offer, "" };

        m_server->registerTransfer(offer.transferId,
                                   tokenBase64,
                                   tokenBytes,
                                   fullPath.toStdString(),
                                   offer.size,
                                   offer.sha256,
                                   TransferDirection::MobileUploadToPc);

        // Envia file_ready com porta e token
        nlohmann::json readyMsg = {
            {"type", "file_ready"},
            {"transferId", offer.transferId},
            {"port", kFileTransferPort},
            {"token", tokenBase64}
        };
        m_wsServer->sendTo(sessionId, readyMsg.dump());

    } catch (const std::exception& e) {
        qWarning() << "FileTransferManager: erro ao processar file_offer:" << e.what();
    }
}

void FileTransferManager::sendFiles(uint64_t sessionId, const QStringList& paths)
{
    if (sessionId == 0 && m_currentSessionId != 0) {
        sessionId = m_currentSessionId;
    }
    if (sessionId == 0) {
        qWarning() << "FileTransferManager: nenhum dispositivo conectado para envio!";
        return;
    }

    // Processa os arquivos em fila ou sequencialmente
    for (const auto& path : paths) {
        QFileInfo fi(path);
        if (!fi.exists() || fi.isDir()) continue;

        // Gera UUID v4
        uint8_t uuidBytes[16];
        randombytes_buf(uuidBytes, 16);
        char uuidStr[37];
        snprintf(uuidStr, sizeof(uuidStr),
                 "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                 uuidBytes[0], uuidBytes[1], uuidBytes[2], uuidBytes[3],
                 uuidBytes[4], uuidBytes[5],
                 uuidBytes[6], uuidBytes[7],
                 uuidBytes[8], uuidBytes[9],
                 uuidBytes[10], uuidBytes[11], uuidBytes[12], uuidBytes[13], uuidBytes[14], uuidBytes[15]);

        std::string transferId = uuidStr;

        // Calcula SHA-256 do arquivo
        crypto_hash_sha256_state shaState;
        crypto_hash_sha256_init(&shaState);
        std::ifstream fileStream(path.toStdString(), std::ios::binary);
        if (!fileStream.is_open()) continue;

        char buf[65536];
        while (fileStream.read(buf, sizeof(buf)) || fileStream.gcount() > 0) {
            crypto_hash_sha256_update(&shaState,
                                      reinterpret_cast<const unsigned char*>(buf),
                                      static_cast<unsigned long long>(fileStream.gcount()));
        }
        fileStream.close();

        uint8_t hashResult[crypto_hash_sha256_BYTES];
        crypto_hash_sha256_final(&shaState, hashResult);
        std::ostringstream ss;
        for (size_t i = 0; i < sizeof(hashResult); ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hashResult[i]);
        }
        std::string sha256Hex = ss.str();

        FileOffer offer;
        offer.transferId = transferId;
        offer.name       = fi.fileName().toStdString();
        offer.size       = static_cast<uint64_t>(fi.size());
        offer.mime       = "application/octet-stream";
        offer.sha256     = sha256Hex;

        m_activeOffers[transferId] = TransferMeta{ sessionId, offer, path.toStdString() };

        // Registra sessão no servidor TCP aguardando conexão do mobile (direction = 1)
        std::vector<uint8_t> tokenBytes;
        std::string tokenBase64 = generateToken(tokenBytes);

        m_server->registerTransfer(transferId,
                                   tokenBase64,
                                   tokenBytes,
                                   path.toStdString(),
                                   offer.size,
                                   sha256Hex,
                                   TransferDirection::DesktopDownloadToMobile);

        // Envia file_offer ao mobile via WebSocket
        nlohmann::json offerMsg = offer;
        m_wsServer->sendTo(sessionId, offerMsg.dump());

        qDebug() << "FileTransferManager: file_offer enviado ao celular para" << fi.fileName();
    }
}

void FileTransferManager::handleFileAccept(uint64_t sessionId, const nlohmann::json& payload)
{
    auto transferId = payload.value("transferId", "");
    if (transferId.empty()) return;

    auto it = m_activeOffers.find(transferId);
    if (it != m_activeOffers.end()) {
        std::vector<uint8_t> tokenBytes;
        std::string tokenBase64 = generateToken(tokenBytes);

        // Atualiza a sessão com o token novo e o caminho completo real do arquivo
        m_server->registerTransfer(transferId,
                                   tokenBase64,
                                   tokenBytes,
                                   it->second.filePath,
                                   it->second.offer.size,
                                   it->second.offer.sha256,
                                   TransferDirection::DesktopDownloadToMobile);

        nlohmann::json readyMsg = {
            {"type", "file_ready"},
            {"transferId", transferId},
            {"port", kFileTransferPort},
            {"token", tokenBase64}
        };
        m_wsServer->sendTo(sessionId, readyMsg.dump());
        qDebug() << "FileTransferManager: file_ready enviado ao celular para transferId" << transferId.c_str();
    }
}

void FileTransferManager::handleFileReject(uint64_t sessionId, const nlohmann::json& payload)
{
    (void)sessionId;
    auto transferId = payload.value("transferId", "");
    if (!transferId.empty()) {
        m_server->cancelTransfer(transferId);
        m_activeOffers.erase(transferId);
    }
}

void FileTransferManager::handleFileComplete(uint64_t sessionId, const nlohmann::json& payload)
{
    (void)sessionId;
    auto transferId = payload.value("transferId", "");
    bool ok = payload.value("ok", false);
    auto error = payload.value("error", "");

    auto it = m_activeOffers.find(transferId);
    if (it != m_activeOffers.end()) {
        emit transferFinished(QString::fromStdString(transferId),
                              QString::fromStdString(it->second.offer.name),
                              ok,
                              QString::fromStdString(error));
        m_activeOffers.erase(it);
    }
}

void FileTransferManager::handleFileCancel(uint64_t sessionId, const nlohmann::json& payload)
{
    (void)sessionId;
    auto transferId = payload.value("transferId", "");
    if (!transferId.empty()) {
        m_server->cancelTransfer(transferId);
        m_activeOffers.erase(transferId);
    }
}

} // namespace Iriseus

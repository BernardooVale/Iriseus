#pragma once
#include <QObject>
#include <QString>
#include <memory>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "FileTransferProtocol.h"
#include "FileTransferServer.h"

class PairingManager;
class WsServer;

namespace Iriseus {

class FileTransferManager : public QObject {
    Q_OBJECT
public:
    explicit FileTransferManager(WsServer* wsServer, PairingManager* pairingManager, QObject* parent = nullptr);
    ~FileTransferManager();

    bool start();
    void stop();

    // Manipuladores de mensagens vindas do WebSocket
    void handleFileOffer(uint64_t sessionId, const nlohmann::json& payload);
    void handleFileAccept(uint64_t sessionId, const nlohmann::json& payload);
    void handleFileReject(uint64_t sessionId, const nlohmann::json& payload);
    void handleFileComplete(uint64_t sessionId, const nlohmann::json& payload);
    void handleFileCancel(uint64_t sessionId, const nlohmann::json& payload);

    QString defaultDownloadDir() const;
    void sendFiles(uint64_t sessionId, const QStringList& paths);
    void setCurrentSession(uint64_t sessionId) { m_currentSessionId = sessionId; }
    uint64_t currentSession() const { return m_currentSessionId; }

signals:
    void transferProgress(QString transferId, quint64 bytes, quint64 total);
    void transferFinished(QString transferId, QString fileName, bool success, QString error);

private:
    std::string generateToken(std::vector<uint8_t>& tokenBytes);
    QString sanitizeFileName(const QString& name);

    WsServer* m_wsServer = nullptr;
    PairingManager* m_pairingManager = nullptr;
    std::unique_ptr<FileTransferServer> m_server;

    // Guarda informações da transferência atual (transferId -> sessionId, etc)
    struct TransferMeta {
        uint64_t sessionId = 0;
        FileOffer offer;
        std::string filePath;
    };
    std::unordered_map<std::string, TransferMeta> m_activeOffers;
    uint64_t m_currentSessionId = 0;
};

} // namespace Iriseus

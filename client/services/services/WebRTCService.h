#ifndef WEBRTCSERVICE_H
#define WEBRTCSERVICE_H

#include <QObject>
#include <QHash>
#include <memory>
#include <string>
#include <rtc/rtc.hpp>

class WebRTCService : public QObject
{
    Q_OBJECT
public:
    explicit WebRTCService(const std::vector<std::string>& stunServers = {}, QObject *parent = nullptr);
    ~WebRTCService();

    /// Set local peer ID for glare resolution (call after signaling "joined")
    void setLocalPeerId(const QString &peerId);

    // Инициировать P2P-соединение с указанным peerId (создать offer)
    void initiateConnection(const QString &roomId, const QString &peerId);

    // Принять входящее предложение (создать answer)
    void acceptConnection(const QString &roomId, const QString &peerId, const QString &sdp);

    // Добавить удалённый ICE-кандидат
    void addRemoteCandidate(const QString &peerId, const QString &candidate, const QString &mid);

    // Отправить сообщение через DataChannel
    void sendMessage(const QString &peerId, const QByteArray &data);

    // Проверить готовность DataChannel
    bool isDataChannelOpen(const QString &peerId) const;

    // Закрыть соединение
    void closeConnection(const QString &peerId);

public slots:
    // Слоты для приёма входящих сигнальных сообщений (вызываются контроллером)
    void onOfferReceived(const QString &roomId, const QString &fromPeerId, const QJsonObject &sdp);
    void onAnswerReceived(const QString &roomId, const QString &fromPeerId, const QJsonObject &sdp);
    void onIceCandidateReceived(const QString &roomId, const QString &fromPeerId, const QJsonObject &candidate);
    void setStunServers(const std::vector<std::string>& servers);

signals:
    // Сигналы для UI
    void connectionOpened(const QString &peerId);
    void connectionClosed(const QString &peerId);
    void messageReceived(const QString &peerId, const QByteArray &data);
    void errorOccurred(const QString &peerId, const QString &error);

    // Сигналы для отправки сигнальных сообщений (контроллер свяжет с SignalingService)
    void sendOffer(const QString &roomId, const QString &peerId, const QJsonObject &sdp);
    void sendAnswer(const QString &roomId, const QString &peerId, const QJsonObject &sdp);
    void sendIceCandidate(const QString &roomId, const QString &peerId, const QJsonObject &candidate);

    private:
    struct PeerConnection {
        std::shared_ptr<rtc::PeerConnection> pc;
        std::shared_ptr<rtc::DataChannel> dc;
        QString roomId;
        QString peerId;
        bool isInitiator = false;
        std::vector<std::pair<std::string, std::string>> pendingCandidates;
    };

    std::vector<std::string> m_stunServers;
    QString m_localPeerId;
    void setupPeerConnection(const QString &roomId, const QString &peerId, bool isInitiator);
    void setupDataChannel(std::shared_ptr<rtc::DataChannel> dc, const QString &peerId);
    void closePeerConnection(const QString &peerId);

    QHash<QString, PeerConnection> m_connections;
};

#endif // WEBRTCSERVICE_H

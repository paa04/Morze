#ifndef SIGNALINGSERVICE_H
#define SIGNALINGSERVICE_H

#include <QObject>
#include <QWebSocket>
#include <QUrl>
#include <QJsonObject>
#include <QJsonArray>

class SignalingService : public QObject
{
    Q_OBJECT
public:
    explicit SignalingService(QObject *parent = nullptr);
    ~SignalingService();

    void connectToServer(const QUrl &url);
    void disconnectFromServer();
    void reconnect();

    bool isConnected() const;

    // === Методы протокола ===
    void join(const QString &roomId, const QString &username, const QString &roomType = "direct");
    void offer(const QString &roomId, const QString &toPeerId, const QJsonObject &sdp);
    void answer(const QString &roomId, const QString &toPeerId, const QJsonObject &sdp);
    void iceCandidate(const QString &roomId, const QString &toPeerId, const QJsonObject &candidate);
    void groupMessage(const QString &roomId, const QJsonObject &payload);
    void ack(const QString &roomId, qint64 upToSeq);
    void leave(const QString &roomId, const QString &peerId);

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &errorString);

    // Входящие сообщения
    void joined(const QString &roomId, const QString &roomType, const QString &peerId, const QJsonArray &participants);
    void peerJoined(const QString &roomId, const QString &peerId, const QString &username);
    void peerLeft(const QString &roomId, const QString &peerId);
    void offerReceived(const QString &roomId, const QString &fromPeerId, const QJsonObject &sdp);
    void answerReceived(const QString &roomId, const QString &fromPeerId, const QJsonObject &sdp);
    void iceCandidateReceived(const QString &roomId, const QString &fromPeerId, const QJsonObject &candidate);
    void groupMessageReceived(const QString &roomId, const QString &fromPeerId, qint64 messageSeq, const QJsonObject &payload);
    void bufferedMessagesReceived(const QString &roomId, const QJsonArray &messages);
    void ackConfirmReceived(const QString &roomId, qint64 upToSeq);
    void errorReceived(const QString &message);

private slots:
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void onTextMessageReceived(const QString &message);

private:
    void sendMessage(const QJsonObject &message);
    void handleMessage(const QJsonObject &msg);

    QWebSocket *m_socket;
    QUrl m_serverUrl;
    bool m_manualDisconnect = false;
};

#endif // SIGNALINGSERVICE_H
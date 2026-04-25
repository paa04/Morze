#include "SignalingService.h"
#include <QTimer>
#include <QJsonDocument>

SignalingService::SignalingService(QObject *parent)
    : QObject(parent)
    , m_socket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
{
    connect(m_socket, &QWebSocket::connected, this, &SignalingService::onConnected);
    connect(m_socket, &QWebSocket::disconnected, this, &SignalingService::onDisconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, &SignalingService::onError);
    connect(m_socket, &QWebSocket::textMessageReceived,
            this, &SignalingService::onTextMessageReceived);
}

SignalingService::~SignalingService()
{
    m_manualDisconnect = true;
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->close();
}

void SignalingService::connectToServer(const QUrl &url)
{
    m_serverUrl = url;
    m_manualDisconnect = false;
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->close();
    m_socket->open(url);
}

void SignalingService::disconnectFromServer()
{
    m_manualDisconnect = true;
    m_socket->close();
}

void SignalingService::reconnect()
{
    if (m_serverUrl.isEmpty() || m_manualDisconnect)
        return;
    if (m_reconnectAttempts >= kMaxReconnectAttempts) {
        emit errorOccurred("Max reconnect attempts reached (" + QString::number(kMaxReconnectAttempts) + ")");
        return;
    }
    int delayMs = std::min(kBaseReconnectMs * (1 << m_reconnectAttempts), kMaxReconnectMs);
    ++m_reconnectAttempts;
    QTimer::singleShot(delayMs, this, [this]() {
        if (m_socket->state() == QAbstractSocket::UnconnectedState) {
            m_socket->open(m_serverUrl);
        }
    });
}

bool SignalingService::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void SignalingService::sendMessage(const QJsonObject &message)
{
    if (!isConnected()) {
        emit errorOccurred("WebSocket not connected");
        return;
    }
    QJsonDocument doc(message);
    m_socket->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

void SignalingService::join(const QString &roomId, const QString &username, const QString &roomType)
{
    QJsonObject msg{{"type", "join"}, {"roomId", roomId}, {"username", username}};
    if (!roomType.isEmpty()) msg["roomType"] = roomType;
    sendMessage(msg);
}

void SignalingService::offer(const QString &roomId, const QString &toPeerId, const QJsonObject &sdp)
{
    sendMessage({{"type", "offer"}, {"roomId", roomId}, {"toPeerId", toPeerId}, {"sdp", sdp}});
}

void SignalingService::answer(const QString &roomId, const QString &toPeerId, const QJsonObject &sdp)
{
    sendMessage({{"type", "answer"}, {"roomId", roomId}, {"toPeerId", toPeerId}, {"sdp", sdp}});
}

void SignalingService::iceCandidate(const QString &roomId, const QString &toPeerId, const QJsonObject &candidate)
{
    sendMessage({{"type", "ice-candidate"}, {"roomId", roomId}, {"toPeerId", toPeerId}, {"candidate", candidate}});
}

void SignalingService::groupMessage(const QString &roomId, const QJsonObject &payload)
{
    sendMessage({{"type", "group-message"}, {"roomId", roomId}, {"payload", payload}});
}

void SignalingService::ack(const QString &roomId, qint64 upToSeq)
{
    sendMessage({{"type", "ack"}, {"roomId", roomId}, {"upToSeq", upToSeq}});
}

void SignalingService::leave(const QString &roomId, const QString &peerId)
{
    sendMessage({{"type", "leave"}, {"roomId", roomId}, {"peerId", peerId}});
}

void SignalingService::onConnected()
{
    m_reconnectAttempts = 0;
    emit connected();
}

void SignalingService::onDisconnected()
{
    emit disconnected();
    if (!m_manualDisconnect)
        reconnect();
}

void SignalingService::onError(QAbstractSocket::SocketError error)
{
    emit errorOccurred(m_socket->errorString());
}

void SignalingService::onTextMessageReceived(const QString &message)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        emit errorOccurred("JSON parse error: " + parseError.errorString());
        return;
    }
    if (!doc.isObject()) {
        emit errorOccurred("JSON is not an object");
        return;
    }
    handleMessage(doc.object());
}

void SignalingService::handleMessage(const QJsonObject &msg)
{
    QString type = msg["type"].toString();
    if (type == "joined") {
        emit joined(msg["roomId"].toString(),
                    msg["roomType"].toString(),
                    msg["peerId"].toString(),
                    msg["participants"].toArray());
    } else if (type == "peer-joined") {
        emit peerJoined(msg["roomId"].toString(),
                        msg["peerId"].toString(),
                        msg["username"].toString());
    } else if (type == "peer-left") {
        emit peerLeft(msg["roomId"].toString(),
                      msg["peerId"].toString());
    } else if (type == "offer") {
        emit offerReceived(msg["roomId"].toString(),
                           msg["fromPeerId"].toString(),
                           msg["sdp"].toObject());
    } else if (type == "answer") {
        emit answerReceived(msg["roomId"].toString(),
                            msg["fromPeerId"].toString(),
                            msg["sdp"].toObject());
    } else if (type == "ice-candidate") {
        emit iceCandidateReceived(msg["roomId"].toString(),
                                  msg["fromPeerId"].toString(),
                                  msg["candidate"].toObject());
    } else if (type == "group-message") {
        emit groupMessageReceived(msg["roomId"].toString(),
                                  msg["fromPeerId"].toString(),
                                  msg["messageSeq"].toInteger(),
                                  msg["payload"].toObject());
    } else if (type == "buffered-messages") {
        emit bufferedMessagesReceived(msg["roomId"].toString(),
                                      msg["messages"].toArray());
    } else if (type == "ack-confirm") {
        emit ackConfirmReceived(msg["roomId"].toString(),
                                msg["upToSeq"].toInteger());
    } else if (type == "error") {
        emit errorReceived(msg["message"].toString());
    }
}
#include "CommunicationController.h"
#include "TimePointConverter.h"

CommunicationController::CommunicationController(SignalingService *signaling,
                                                 WebRTCService *webRTC,
                                                 QObject *parent)
    : QObject(parent)
    , m_signaling(signaling)
    , m_webRTC(webRTC)
{
    // Подключаем сигналы от SignalingService
    connect(m_signaling, &SignalingService::connected, this, &CommunicationController::onSignalingConnected);
    connect(m_signaling, &SignalingService::disconnected, this, &CommunicationController::onSignalingDisconnected);
    connect(m_signaling, &SignalingService::joined, this, &CommunicationController::onSignalingJoined);
    connect(m_signaling, &SignalingService::peerJoined, this, &CommunicationController::onSignalingPeerJoined);
    connect(m_signaling, &SignalingService::peerLeft, this, &CommunicationController::onSignalingPeerLeft);
    connect(m_signaling, &SignalingService::offerReceived, this, &CommunicationController::onSignalingOfferReceived);
    connect(m_signaling, &SignalingService::answerReceived, this, &CommunicationController::onSignalingAnswerReceived);
    connect(m_signaling, &SignalingService::iceCandidateReceived, this, &CommunicationController::onSignalingIceCandidateReceived);
    connect(m_signaling, &SignalingService::groupMessageReceived, this, &CommunicationController::onSignalingGroupMessageReceived);
    connect(m_signaling, &SignalingService::bufferedMessagesReceived, this, &CommunicationController::onSignalingBufferedMessagesReceived);
    connect(m_signaling, &SignalingService::errorReceived, this, &CommunicationController::onSignalingErrorReceived);

    // Подключаем сигналы от WebRTCService
    connect(m_webRTC, &WebRTCService::connectionOpened, this, &CommunicationController::onWebRtcConnectionOpened);
    connect(m_webRTC, &WebRTCService::connectionClosed, this, &CommunicationController::onWebRtcConnectionClosed);
    connect(m_webRTC, &WebRTCService::messageReceived, this, &CommunicationController::onWebRtcMessageReceived);
    connect(m_webRTC, &WebRTCService::errorOccurred, this, &CommunicationController::onWebRtcError);
    connect(m_webRTC, &WebRTCService::sendOffer, this, &CommunicationController::onWebRtcSendOffer);
    connect(m_webRTC, &WebRTCService::sendAnswer, this, &CommunicationController::onWebRtcSendAnswer);
    connect(m_webRTC, &WebRTCService::sendIceCandidate, this, &CommunicationController::onWebRtcSendIceCandidate);
}

void CommunicationController::joinChat(const ChatDTO& chat, const std::string& username)
{
    m_currentUsername = username;
    m_signaling->join(QString::fromStdString(chat.getRoomId()),
                      QString::fromStdString(username),
                      QString::fromStdString(chat.getType()));

    RoomState state;
    state.roomId = chat.getRoomId();
    state.roomType = chat.getType();
    state.username = username;
    m_rooms[chat.getRoomId()] = state;
}

void CommunicationController::leaveChat(const std::string& roomId)
{
    auto it = m_rooms.find(roomId);
    if (it == m_rooms.end()) return;

    m_signaling->leave(QString::fromStdString(roomId), QString::fromStdString(it->second.myPeerId));

    for (const auto& [peerId, _] : it->second.participants) {
        m_webRTC->closeConnection(QString::fromStdString(peerId));
        m_peerToRoom.erase(peerId);
    }
    m_rooms.erase(it);
    emit chatLeft(roomId);
}

void CommunicationController::sendMessage(const MessageDTO& msg)
{
    auto it = m_rooms.find(msg.getChatId());
    if (it == m_rooms.end()) {
        emit errorOccurred("Not in chat room " + msg.getChatId());
        return;
    }

    if (it->second.roomType == "direct") {
        handleDirectMessage(msg);
    } else {
        handleGroupMessage(msg);
    }
}

void CommunicationController::updateConnectionProfile(const std::string& serverUrl, const std::string& stunUrl)
{
    m_currentServerUrl = serverUrl;
    m_currentStunUrl = stunUrl;
    m_signaling->disconnectFromServer();
    m_signaling->connectToServer(QUrl(QString::fromStdString(serverUrl)));
}

std::vector<ChatMemberDTO> CommunicationController::getParticipants(const std::string& roomId) const
{
    std::vector<ChatMemberDTO> result;
    auto it = m_rooms.find(roomId);
    if (it != m_rooms.end()) {
        for (const auto& [_, member] : it->second.participants) {
            result.push_back(member);
        }
    }
    return result;
}

// --- Входящие от SignalingService ---
void CommunicationController::onSignalingConnected()
{
    // Re-join all rooms we were in before disconnect (API.md: "Чек-лист для запуска клиента")
    for (auto& [roomId, state] : m_rooms) {
        if (state.username.empty()) continue;
        m_signaling->join(QString::fromStdString(roomId),
                          QString::fromStdString(state.username),
                          QString::fromStdString(state.roomType));
    }
}

void CommunicationController::onSignalingDisconnected()
{
    // Close all WebRTC connections — they're dead without signaling
    for (auto& [roomId, state] : m_rooms) {
        for (const auto& [peerId, _] : state.participants) {
            m_webRTC->closeConnection(QString::fromStdString(peerId));
        }
        state.participants.clear();
        state.myPeerId.clear();
    }
    m_peerToRoom.clear();
    m_pendingMessages.clear();
}

void CommunicationController::onSignalingJoined(const QString& roomId, const QString& roomType,
                                                const QString& peerId, const QJsonArray& participants)
{
    std::string roomIdStr = roomId.toStdString();
    auto it = m_rooms.find(roomIdStr);
    if (it == m_rooms.end()) return;

    it->second.myPeerId = peerId.toStdString();
    it->second.roomType = roomType.toStdString();

    for (const QJsonValue& v : participants) {
        QJsonObject obj = v.toObject();
        ChatMemberDTO member;
        member.setId(obj["peerId"].toString().toStdString());
        member.setUsername(obj["username"].toString().toStdString());
        it->second.participants[member.getId()] = member;
        m_peerToRoom[member.getId()] = roomIdStr;
    }

    ChatDTO chat;
    chat.setRoomId(roomIdStr);
    chat.setType(roomType.toStdString());
    emit chatJoined(chat, peerId.toStdString());
}

void CommunicationController::onSignalingPeerJoined(const QString& roomId, const QString& peerId, const QString& username)
{
    std::string roomIdStr = roomId.toStdString();
    std::string peerIdStr = peerId.toStdString();
    auto it = m_rooms.find(roomIdStr);
    if (it == m_rooms.end()) return;

    ChatMemberDTO member;
    member.setId(peerIdStr);
    member.setUsername(username.toStdString());
    it->second.participants[peerIdStr] = member;
    m_peerToRoom[peerIdStr] = roomIdStr;

    emit participantJoined(roomIdStr, member);

    if (it->second.roomType == "direct"
        && !it->second.myPeerId.empty()
        && peerIdStr != it->second.myPeerId) {
        setupWebRtcForPeer(roomIdStr, peerIdStr);
    }
}

void CommunicationController::onSignalingPeerLeft(const QString& roomId, const QString& peerId)
{
    std::string roomIdStr = roomId.toStdString();
    std::string peerIdStr = peerId.toStdString();
    auto it = m_rooms.find(roomIdStr);
    if (it == m_rooms.end()) return;

    it->second.participants.erase(peerIdStr);
    m_peerToRoom.erase(peerIdStr);
    m_webRTC->closeConnection(peerId);

    emit participantLeft(roomIdStr, peerIdStr);
}

void CommunicationController::onSignalingOfferReceived(const QString& roomId, const QString& fromPeerId, const QJsonObject& sdp)
{
    m_webRTC->onOfferReceived(roomId, fromPeerId, sdp);
}

void CommunicationController::onSignalingAnswerReceived(const QString& roomId, const QString& fromPeerId, const QJsonObject& sdp)
{
    m_webRTC->onAnswerReceived(roomId, fromPeerId, sdp);
}

void CommunicationController::onSignalingIceCandidateReceived(const QString& roomId, const QString& fromPeerId, const QJsonObject& candidate)
{
    m_webRTC->onIceCandidateReceived(roomId, fromPeerId, candidate);
}

void CommunicationController::onSignalingGroupMessageReceived(const QString& roomId, const QString& fromPeerId,
                                                              qint64 seq, const QJsonObject& payload)
{
    MessageDTO msg;
    msg.setChatId(roomId.toStdString());
    msg.setSenderId(fromPeerId.toStdString());
    msg.setContent(payload["text"].toString().toStdString());
    msg.setCreatedAt(TimePointConverter::toIsoString(std::chrono::system_clock::now()));
    msg.setDirection("incoming");
    msg.setDeliveryState("delivered");

    emit messageReceived(msg);
    m_signaling->ack(roomId, seq);
}

void CommunicationController::onSignalingBufferedMessagesReceived(const QString& roomId, const QJsonArray& messages)
{
    std::cout << "[BUFFERED] Received " << messages.size() << " buffered message(s) for room " << roomId.toStdString() << "\n";
    std::cout.flush();
    for (const QJsonValue& v : messages) {
        QJsonObject obj = v.toObject();
        onSignalingGroupMessageReceived(roomId,
                                        obj["fromPeerId"].toString(),
                                        obj["messageSeq"].toInteger(),
                                        obj["payload"].toObject());
    }
}

void CommunicationController::onSignalingErrorReceived(const QString& error)
{
    emit errorOccurred(error.toStdString());
}

// --- WebRTC ---
void CommunicationController::onWebRtcConnectionOpened(const QString& peerId) {
    flushPendingMessages(peerId.toStdString());
}
void CommunicationController::onWebRtcConnectionClosed(const QString& peerId) {}

void CommunicationController::onWebRtcMessageReceived(const QString& peerId, const QByteArray& data)
{
    auto it = m_peerToRoom.find(peerId.toStdString());
    if (it == m_peerToRoom.end()) return;

    MessageDTO msg;
    msg.setChatId(it->second);
    msg.setSenderId(peerId.toStdString());
    msg.setContent(data.toStdString());
    msg.setDirection("incoming");
    msg.setDeliveryState("delivered");
    msg.setCreatedAt(TimePointConverter::toIsoString(std::chrono::system_clock::now()));

    emit messageReceived(msg);
}

void CommunicationController::onWebRtcError(const QString& peerId, const QString& error)
{
    emit errorOccurred("WebRTC error with " + peerId.toStdString() + ": " + error.toStdString());
}

void CommunicationController::onWebRtcSendOffer(const QString& roomId, const QString& peerId, const QJsonObject& sdp)
{
    m_signaling->offer(roomId, peerId, sdp);
}

void CommunicationController::onWebRtcSendAnswer(const QString& roomId, const QString& peerId, const QJsonObject& sdp)
{
    m_signaling->answer(roomId, peerId, sdp);
}

void CommunicationController::onWebRtcSendIceCandidate(const QString& roomId, const QString& peerId, const QJsonObject& candidate)
{
    m_signaling->iceCandidate(roomId, peerId, candidate);
}

// --- Приватные ---
void CommunicationController::handleDirectMessage(const MessageDTO& msg)
{
    auto it = m_rooms.find(msg.getChatId());
    if (it == m_rooms.end()) return;

    std::string targetPeerId;
    for (const auto& [peerId, _] : it->second.participants) {
        if (peerId != it->second.myPeerId) {
            targetPeerId = peerId;
            break;
        }
    }
    if (targetPeerId.empty()) {
        emit errorOccurred("No peer to send message");
        return;
    }

    ensureWebRtcConnection(msg.getChatId(), targetPeerId);

    QByteArray data = QByteArray::fromStdString(msg.getContent());
    // Queue message — will be sent when DataChannel opens (or immediately if already open)
    auto &queue = m_pendingMessages[targetPeerId];
    if (queue.size() >= 1000) {
        emit errorOccurred("Message queue full for peer " + targetPeerId + ", dropping oldest");
        queue.erase(queue.begin());
    }
    queue.push_back(data);
    flushPendingMessages(targetPeerId);
    emit messageDelivered(msg.getId());
}

void CommunicationController::flushPendingMessages(const std::string& peerId)
{
    auto it = m_pendingMessages.find(peerId);
    if (it == m_pendingMessages.end() || it->second.empty()) return;

    if (!m_webRTC->isDataChannelOpen(QString::fromStdString(peerId)))
        return; // DC not ready yet — keep messages queued

    for (auto& data : it->second) {
        m_webRTC->sendMessage(QString::fromStdString(peerId), data);
    }
    it->second.clear();
}

void CommunicationController::handleGroupMessage(const MessageDTO& msg)
{
    QJsonObject payload{{"text", QString::fromStdString(msg.getContent())}};
    m_signaling->groupMessage(QString::fromStdString(msg.getChatId()), payload);
    emit messageDelivered(msg.getId());
}

void CommunicationController::setupWebRtcForPeer(const std::string& roomId, const std::string& peerId)
{
    m_webRTC->initiateConnection(QString::fromStdString(roomId), QString::fromStdString(peerId));
}

void CommunicationController::ensureWebRtcConnection(const std::string& roomId, const std::string& peerId)
{
    // initiateConnection returns early if connection already exists
    m_webRTC->initiateConnection(QString::fromStdString(roomId), QString::fromStdString(peerId));
}
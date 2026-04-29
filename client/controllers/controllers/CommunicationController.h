#ifndef COMMUNICATIONCONTROLLER_H
#define COMMUNICATIONCONTROLLER_H

#include <string>


#include "ChatDTO.h"
#include "MessageDTO.h"
#include "ChatMemberDTO.h"
#include "SignalingService.h"
#include "WebRTCService.h"
class SignalingService;
class WebRTCService;

class CommunicationController : public QObject
{
    Q_OBJECT
public:
    explicit CommunicationController(SignalingService *signaling,
                                     WebRTCService *webRTC,
                                     QObject *parent = nullptr);

    // Команды от UI
    void joinChat(const ChatDTO& chat, const std::string& username);
    void leaveChat(const std::string& roomId);
    void sendMessage(const MessageDTO& msg);
    void updateConnectionProfile(const std::string& serverUrl, const std::string& stunUrl);

    // Запрос состояния
    std::vector<ChatMemberDTO> getParticipants(const std::string& roomId) const;

signals:
    // События для UI (передаём DTO)
    void chatJoined(const ChatDTO& chat, const std::string& myPeerId);
    void chatLeft(const std::string& roomId);
    void participantJoined(const std::string& roomId, const ChatMemberDTO& participant);
    void participantLeft(const std::string& roomId, const std::string& peerId);
    void messageReceived(const MessageDTO& msg);
    void messageDelivered(const std::string& messageId);
    void errorOccurred(const std::string& error);

private slots:
    // Обработчики сигналов от SignalingService
    void onSignalingConnected();
    void onSignalingDisconnected();
    void onSignalingJoined(const QString& roomId, const QString& roomType,
                           const QString& peerId, const QJsonArray& participants);
    void onSignalingPeerJoined(const QString& roomId, const QString& peerId, const QString& username);
    void onSignalingPeerLeft(const QString& roomId, const QString& peerId);
    void onSignalingOfferReceived(const QString& roomId, const QString& fromPeerId, const QJsonObject& sdp);
    void onSignalingAnswerReceived(const QString& roomId, const QString& fromPeerId, const QJsonObject& sdp);
    void onSignalingIceCandidateReceived(const QString& roomId, const QString& fromPeerId, const QJsonObject& candidate);
    void onSignalingGroupMessageReceived(const QString& roomId, const QString& fromPeerId,
                                         qint64 seq, const QJsonObject& payload);
    void onSignalingBufferedMessagesReceived(const QString& roomId, const QJsonArray& messages);
    void onSignalingErrorReceived(const QString& error);

    // Обработчики сигналов от WebRTCService
    void onWebRtcConnectionOpened(const QString& peerId);
    void onWebRtcConnectionClosed(const QString& peerId);
    void onWebRtcMessageReceived(const QString& peerId, const QByteArray& data);
    void onWebRtcError(const QString& peerId, const QString& error);
    void onWebRtcSendOffer(const QString& roomId, const QString& peerId, const QJsonObject& sdp);
    void onWebRtcSendAnswer(const QString& roomId, const QString& peerId, const QJsonObject& sdp);
    void onWebRtcSendIceCandidate(const QString& roomId, const QString& peerId, const QJsonObject& candidate);

private:
    struct RoomState {
        std::string roomId;
        std::string roomType;
        std::string myPeerId;
        std::string username; // saved for reconnect
        std::unordered_map<std::string, ChatMemberDTO> participants; // peerId -> DTO
    };

    void handleDirectMessage(const MessageDTO& msg);
    void handleGroupMessage(const MessageDTO& msg);
    void setupWebRtcForPeer(const std::string& roomId, const std::string& peerId);
    void ensureWebRtcConnection(const std::string& roomId, const std::string& peerId);

    void flushPendingMessages(const std::string& peerId);

    SignalingService *m_signaling;
    WebRTCService *m_webRTC;

    std::unordered_map<std::string, RoomState> m_rooms; // roomId -> состояние
    std::unordered_map<std::string, std::string> m_peerToRoom; // peerId -> roomId
    // peerId -> queued messages waiting for DataChannel to open
    std::unordered_map<std::string, std::vector<QByteArray>> m_pendingMessages;

    std::string m_currentServerUrl;
    std::string m_currentStunUrl;
    std::string m_currentUsername;
};

#endif // COMMUNICATIONCONTROLLER_H
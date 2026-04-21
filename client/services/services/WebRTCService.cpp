#include "WebRTCService.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

WebRTCService::WebRTCService(QObject *parent)
    : QObject(parent)
{
}

WebRTCService::~WebRTCService()
{
    for (const auto &peerId : m_connections.keys())
        closePeerConnection(peerId);
}

void WebRTCService::initiateConnection(const QString &roomId, const QString &peerId)
{
    if (m_connections.contains(peerId)) {
        qWarning() << "Connection already exists for" << peerId;
        return;
    }
    setupPeerConnection(roomId, peerId, true);
}

void WebRTCService::acceptConnection(const QString &roomId, const QString &peerId, const QString &sdp)
{
    if (m_connections.contains(peerId)) {
        qWarning() << "Connection already exists for" << peerId;
        return;
    }
    setupPeerConnection(roomId, peerId, false);
    auto &conn = m_connections[peerId];
    conn.pc->setRemoteDescription(rtc::Description(sdp.toStdString(), "offer"));
}

void WebRTCService::addRemoteCandidate(const QString &peerId, const QString &candidate, const QString &mid)
{
    auto it = m_connections.find(peerId);
    if (it == m_connections.end()) {
        qWarning() << "No connection for" << peerId;
        return;
    }
    it->pc->addRemoteCandidate(rtc::Candidate(candidate.toStdString(), mid.toStdString()));
}

void WebRTCService::sendMessage(const QString &peerId, const QByteArray &data)
{
    auto it = m_connections.find(peerId);
    if (it == m_connections.end() || !it->dc || !it->dc->isOpen()) {
        emit errorOccurred(peerId, "DataChannel not open");
        return;
    }
    it->dc->send(std::vector<std::byte>(reinterpret_cast<const std::byte*>(data.constData()),
                                       reinterpret_cast<const std::byte*>(data.constData() + data.size())));
}

void WebRTCService::closeConnection(const QString &peerId)
{
    closePeerConnection(peerId);
}

void WebRTCService::onOfferReceived(const QString &roomId, const QString &fromPeerId, const QJsonObject &sdp)
{
    // Здесь можно автоматически принять или просто передать сигнал UI.
    // Пока просто логируем, контроллер сам вызовет acceptConnection при необходимости.
    qDebug() << "Received offer from" << fromPeerId << "in room" << roomId;
}

void WebRTCService::onAnswerReceived(const QString &roomId, const QString &fromPeerId, const QJsonObject &sdp)
{
    auto it = m_connections.find(fromPeerId);
    if (it == m_connections.end()) {
        qWarning() << "No connection for answer from" << fromPeerId;
        return;
    }
    it->pc->setRemoteDescription(rtc::Description(sdp["sdp"].toString().toStdString(), "answer"));
}

void WebRTCService::onIceCandidateReceived(const QString &roomId, const QString &fromPeerId, const QJsonObject &candidate)
{
    addRemoteCandidate(fromPeerId, candidate["candidate"].toString(), candidate["sdpMid"].toString());
}

void WebRTCService::setupPeerConnection(const QString &roomId, const QString &peerId, bool isInitiator)
{
    rtc::Configuration config;
    config.iceServers.emplace_back("stun:stun.l.google.com:19302");
    // config.iceServers.emplace_back("stun:global.stun.twilio.com:3478"); // резервный

    auto pc = std::make_shared<rtc::PeerConnection>(config);
    std::shared_ptr<rtc::DataChannel> dc;

    if (isInitiator) {
        dc = pc->createDataChannel("chat");
        setupDataChannel(dc, peerId);
    } else {
        pc->onDataChannel([this, peerId](std::shared_ptr<rtc::DataChannel> incoming) {
            setupDataChannel(incoming, peerId);
        });
    }

    pc->onLocalDescription([this, roomId, peerId](rtc::Description desc) {
        QJsonObject sdp{
            {"type", desc.typeString().c_str()},
            {"sdp", desc.c_str()}
        };
        if (desc.type() == rtc::Description::Type::Offer) {
            emit sendOffer(roomId, peerId, sdp);
        } else {
            emit sendAnswer(roomId, peerId, sdp);
        }
    });

    pc->onLocalCandidate([this, roomId, peerId](rtc::Candidate cand) {
        QJsonObject candidate{
            {"candidate", cand.candidate().c_str()},
            {"sdpMid", cand.mid().c_str()}
        };
        emit sendIceCandidate(roomId, peerId, candidate);
    });

    pc->onStateChange([this, peerId](rtc::PeerConnection::State state) {
        if (state == rtc::PeerConnection::State::Connected ||
            state == rtc::PeerConnection::State::Completed) {
            emit connectionOpened(peerId);
        } else if (state == rtc::PeerConnection::State::Disconnected ||
                   state == rtc::PeerConnection::State::Failed ||
                   state == rtc::PeerConnection::State::Closed) {
            emit connectionClosed(peerId);
            closePeerConnection(peerId);
        }
    });

    PeerConnection conn{pc, dc, roomId, peerId, isInitiator};
    m_connections[peerId] = conn;
}

void WebRTCService::setupDataChannel(std::shared_ptr<rtc::DataChannel> dc, const QString &peerId)
{
    dc->onOpen([this, peerId]() {
        emit connectionOpened(peerId);
    });

    dc->onMessage([this, peerId](rtc::message_variant data) {
        if (std::holds_alternative<std::string>(data)) {
            const auto &str = std::get<std::string>(data);
            emit messageReceived(peerId, QByteArray(str.data(), str.size()));
        } else if (std::holds_alternative<rtc::binary>(data)) {
            const auto &bin = std::get<rtc::binary>(data);
            emit messageReceived(peerId, QByteArray(reinterpret_cast<const char*>(bin.data()), bin.size()));
        }
    });

    dc->onClosed([this, peerId]() {
        emit connectionClosed(peerId);
    });

    auto it = m_connections.find(peerId);
    if (it != m_connections.end()) {
        it->dc = dc;
    }
}

void WebRTCService::closePeerConnection(const QString &peerId)
{
    auto it = m_connections.find(peerId);
    if (it != m_connections.end()) {
        if (it->pc)
            it->pc->close();
        m_connections.erase(it);
        emit connectionClosed(peerId);
    }
}
#include "WebRTCService.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

WebRTCService::WebRTCService(const std::vector<std::string>& stunServers, QObject *parent)
    : QObject(parent), m_stunServers(stunServers)
{
    // No default STUN — works on localhost without external servers
}

WebRTCService::~WebRTCService()
{
    for (const auto &peerId : m_connections.keys())
        closePeerConnection(peerId);
}

void WebRTCService::setStunServers(const std::vector<std::string>& servers) {
    m_stunServers = servers;
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

bool WebRTCService::isDataChannelOpen(const QString &peerId) const
{
    auto it = m_connections.find(peerId);
    return it != m_connections.end() && it->dc && it->dc->isOpen();
}

void WebRTCService::closeConnection(const QString &peerId)
{
    closePeerConnection(peerId);
}

void WebRTCService::onOfferReceived(const QString &roomId, const QString &fromPeerId, const QJsonObject &sdp)
{
    acceptConnection(roomId, fromPeerId, sdp["sdp"].toString());
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
    for (const auto& url : m_stunServers) {
        config.iceServers.emplace_back(url);
    }

    std::cout << "[WebRTC] Creating PeerConnection (initiator=" << isInitiator
              << ", stunServers=" << m_stunServers.size() << ")\n";
    std::cout.flush();

    auto pc = std::make_shared<rtc::PeerConnection>(config);

    std::cout << "[WebRTC] PeerConnection created, gathering state="
              << static_cast<int>(pc->gatheringState()) << "\n";
    std::cout.flush();

    // Register ALL callbacks BEFORE createDataChannel (which triggers offer generation)
    pc->onLocalDescription([this, roomId, peerId](rtc::Description desc) {
        QJsonObject sdp{
            {"type", desc.typeString().c_str()},
            {"sdp", QString::fromStdString(desc)}
        };
        std::cout << "[WebRTC] Local " << desc.typeString() << " generated for " << peerId.toStdString() << "\n";
        std::cout.flush();
        if (desc.type() == rtc::Description::Type::Offer) {
            QMetaObject::invokeMethod(this, [this, roomId, peerId, sdp]() {
                emit sendOffer(roomId, peerId, sdp);
            }, Qt::QueuedConnection);
        } else {
            QMetaObject::invokeMethod(this, [this, roomId, peerId, sdp]() {
                emit sendAnswer(roomId, peerId, sdp);
            }, Qt::QueuedConnection);
        }
    });

    pc->onLocalCandidate([this, roomId, peerId](rtc::Candidate cand) {
        QJsonObject candidate{
            {"candidate", cand.candidate().c_str()},
            {"sdpMid", cand.mid().c_str()}
        };
        QMetaObject::invokeMethod(this, [this, roomId, peerId, candidate]() {
            emit sendIceCandidate(roomId, peerId, candidate);
        }, Qt::QueuedConnection);
    });

    pc->onStateChange([this, peerId](rtc::PeerConnection::State state) {
        std::cout << "[WebRTC] State: " << static_cast<int>(state) << " for " << peerId.toStdString() << "\n";
        std::cout.flush();
        if (state == rtc::PeerConnection::State::Connected) {
            QMetaObject::invokeMethod(this, [this, peerId]() {
                emit connectionOpened(peerId);
            }, Qt::QueuedConnection);
        } else if (state == rtc::PeerConnection::State::Disconnected ||
                   state == rtc::PeerConnection::State::Failed ||
                   state == rtc::PeerConnection::State::Closed) {
            QMetaObject::invokeMethod(this, [this, peerId]() {
                emit connectionClosed(peerId);
            }, Qt::QueuedConnection);
        }
    });

    // Now create DataChannel / register onDataChannel AFTER all callbacks are set
    std::shared_ptr<rtc::DataChannel> dc;
    if (isInitiator) {
        dc = pc->createDataChannel("chat");
        std::cout << "[WebRTC] DataChannel 'chat' created\n";
        std::cout.flush();
        setupDataChannel(dc, peerId);
    } else {
        pc->onDataChannel([this, peerId](std::shared_ptr<rtc::DataChannel> incoming) {
            QMetaObject::invokeMethod(this, [this, peerId, incoming]() {
                setupDataChannel(incoming, peerId);
            }, Qt::QueuedConnection);
        });
    }

    PeerConnection conn{pc, dc, roomId, peerId, isInitiator};
    m_connections[peerId] = conn;
}

void WebRTCService::setupDataChannel(std::shared_ptr<rtc::DataChannel> dc, const QString &peerId)
{
    dc->onOpen([this, peerId]() {
        std::cout << "[WebRTC] DataChannel opened for " << peerId.toStdString() << "\n";
        std::cout.flush();
        QMetaObject::invokeMethod(this, [this, peerId]() {
            emit connectionOpened(peerId);
        }, Qt::QueuedConnection);
    });

    dc->onMessage([this, peerId](rtc::message_variant data) {
        QByteArray bytes;
        if (std::holds_alternative<std::string>(data)) {
            const auto &str = std::get<std::string>(data);
            bytes = QByteArray(str.data(), str.size());
        } else if (std::holds_alternative<rtc::binary>(data)) {
            const auto &bin = std::get<rtc::binary>(data);
            bytes = QByteArray(reinterpret_cast<const char*>(bin.data()), bin.size());
        }
        QMetaObject::invokeMethod(this, [this, peerId, bytes]() {
            emit messageReceived(peerId, bytes);
        }, Qt::QueuedConnection);
    });

    dc->onClosed([this, peerId]() {
        QMetaObject::invokeMethod(this, [this, peerId]() {
            emit connectionClosed(peerId);
        }, Qt::QueuedConnection);
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
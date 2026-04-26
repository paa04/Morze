#include "WebRTCService.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QDebug>

WebRTCService::WebRTCService(const std::vector<std::string>& stunServers, QObject *parent)
    : QObject(parent), m_stunServers(stunServers)
{
}

WebRTCService::~WebRTCService()
{
    // Close all PeerConnections synchronously so libdatachannel callbacks stop
    // before `this` is destroyed. Reset callbacks first to prevent stale invocations.
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        if (it->pc) {
            it->pc->onLocalDescription(nullptr);
            it->pc->onLocalCandidate(nullptr);
            it->pc->onStateChange(nullptr);
            it->pc->onDataChannel(nullptr);
            if (it->dc) {
                it->dc->onOpen(nullptr);
                it->dc->onMessage(nullptr);
                it->dc->onClosed(nullptr);
            }
            it->pc->close();
        }
    }
    m_connections.clear();
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
    for (const auto &[cand, mid] : conn.pendingCandidates)
        conn.pc->addRemoteCandidate(rtc::Candidate(cand, mid));
    conn.pendingCandidates.clear();
}

void WebRTCService::addRemoteCandidate(const QString &peerId, const QString &candidate, const QString &mid)
{
    auto it = m_connections.find(peerId);
    if (it == m_connections.end()) {
        qWarning() << "No connection for" << peerId;
        return;
    }
    try {
        it->pc->addRemoteCandidate(rtc::Candidate(candidate.toStdString(), mid.toStdString()));
    } catch (const std::exception &e) {
        qWarning() << "addRemoteCandidate deferred for" << peerId << ":" << e.what();
        // ICE candidate arrived before remote description — buffer it
        it->pendingCandidates.emplace_back(candidate.toStdString(), mid.toStdString());
    }
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
    for (const auto &[cand, mid] : it->pendingCandidates)
        it->pc->addRemoteCandidate(rtc::Candidate(cand, mid));
    it->pendingCandidates.clear();
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

    auto pc = std::make_shared<rtc::PeerConnection>(config);

    // QPointer guard: if WebRTCService is destroyed, `guard` becomes null
    // and libdatachannel callbacks safely no-op instead of use-after-free.
    QPointer<WebRTCService> guard(this);

    pc->onLocalDescription([guard, roomId, peerId](rtc::Description desc) {
        if (!guard) return;
        QJsonObject sdp{
            {"type", desc.typeString().c_str()},
            {"sdp", QString::fromStdString(desc)}
        };
        if (desc.type() == rtc::Description::Type::Offer) {
            QMetaObject::invokeMethod(guard.data(), [guard, roomId, peerId, sdp]() {
                if (guard) emit guard->sendOffer(roomId, peerId, sdp);
            }, Qt::QueuedConnection);
        } else {
            QMetaObject::invokeMethod(guard.data(), [guard, roomId, peerId, sdp]() {
                if (guard) emit guard->sendAnswer(roomId, peerId, sdp);
            }, Qt::QueuedConnection);
        }
    });

    pc->onLocalCandidate([guard, roomId, peerId](rtc::Candidate cand) {
        if (!guard) return;
        QJsonObject candidate{
            {"candidate", cand.candidate().c_str()},
            {"sdpMid", cand.mid().c_str()}
        };
        QMetaObject::invokeMethod(guard.data(), [guard, roomId, peerId, candidate]() {
            if (guard) emit guard->sendIceCandidate(roomId, peerId, candidate);
        }, Qt::QueuedConnection);
    });

    pc->onStateChange([guard, peerId](rtc::PeerConnection::State state) {
        if (!guard) return;
        if (state == rtc::PeerConnection::State::Connected) {
            QMetaObject::invokeMethod(guard.data(), [guard, peerId]() {
                if (guard) emit guard->connectionOpened(peerId);
            }, Qt::QueuedConnection);
        } else if (state == rtc::PeerConnection::State::Disconnected ||
                   state == rtc::PeerConnection::State::Failed ||
                   state == rtc::PeerConnection::State::Closed) {
            QMetaObject::invokeMethod(guard.data(), [guard, peerId]() {
                if (guard) emit guard->connectionClosed(peerId);
            }, Qt::QueuedConnection);
        }
    });

    std::shared_ptr<rtc::DataChannel> dc;
    if (isInitiator) {
        dc = pc->createDataChannel("chat");
        setupDataChannel(dc, peerId);
    } else {
        pc->onDataChannel([guard, peerId](std::shared_ptr<rtc::DataChannel> incoming) {
            if (!guard) return;
            QMetaObject::invokeMethod(guard.data(), [guard, peerId, incoming]() {
                if (guard) guard->setupDataChannel(incoming, peerId);
            }, Qt::QueuedConnection);
        });
    }

    PeerConnection conn{pc, dc, roomId, peerId, isInitiator};
    m_connections[peerId] = conn;
}

void WebRTCService::setupDataChannel(std::shared_ptr<rtc::DataChannel> dc, const QString &peerId)
{
    if (!dc) return;

    QPointer<WebRTCService> guard(this);

    dc->onOpen([guard, peerId]() {
        if (!guard) return;
        QMetaObject::invokeMethod(guard.data(), [guard, peerId]() {
            if (guard) emit guard->connectionOpened(peerId);
        }, Qt::QueuedConnection);
    });

    dc->onMessage([guard, peerId](rtc::message_variant data) {
        if (!guard) return;
        QByteArray bytes;
        if (std::holds_alternative<std::string>(data)) {
            const auto &str = std::get<std::string>(data);
            bytes = QByteArray(str.data(), str.size());
        } else if (std::holds_alternative<rtc::binary>(data)) {
            const auto &bin = std::get<rtc::binary>(data);
            bytes = QByteArray(reinterpret_cast<const char*>(bin.data()), bin.size());
        }
        QMetaObject::invokeMethod(guard.data(), [guard, peerId, bytes]() {
            if (guard) emit guard->messageReceived(peerId, bytes);
        }, Qt::QueuedConnection);
    });

    dc->onClosed([guard, peerId]() {
        if (!guard) return;
        QMetaObject::invokeMethod(guard.data(), [guard, peerId]() {
            if (guard) emit guard->connectionClosed(peerId);
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
        // Unregister callbacks before closing to prevent stale invocations
        if (it->dc) {
            it->dc->onOpen(nullptr);
            it->dc->onMessage(nullptr);
            it->dc->onClosed(nullptr);
        }
        if (it->pc) {
            it->pc->onLocalDescription(nullptr);
            it->pc->onLocalCandidate(nullptr);
            it->pc->onStateChange(nullptr);
            it->pc->onDataChannel(nullptr);
            it->pc->close();
        }
        m_connections.erase(it);
        emit connectionClosed(peerId);
    }
}

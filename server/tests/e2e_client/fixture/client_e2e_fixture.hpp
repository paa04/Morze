#pragma once

#include "signaling_server.hpp"
#include "SignalingService.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// TestClient — thin wrapper around real SignalingService that captures signals
// ---------------------------------------------------------------------------
class TestClient {
public:
    TestClient() : service_(std::make_unique<SignalingService>()) { wireSignals(); }

    SignalingService* operator->()        { return service_.get(); }
    SignalingService* get()               { return service_.get(); }

    // --- connection ----------------------------------------------------------
    bool connected = false;
    bool disconnected = false;

    // --- joined --------------------------------------------------------------
    bool joinedReceived = false;
    QString peerId;
    QString roomId;
    QString roomType;
    QJsonArray participants;

    // --- peer events ---------------------------------------------------------
    struct PeerEvent { QString peerId; QString username; };
    std::vector<PeerEvent> peersJoined;
    std::vector<QString>   peersLeft;

    // --- signaling (offer / answer / ice) ------------------------------------
    struct SignalData { QString roomId; QString fromPeerId; QJsonObject payload; };
    std::vector<SignalData> offersReceived;
    std::vector<SignalData> answersReceived;
    std::vector<SignalData> iceCandidatesReceived;

    // --- group messages ------------------------------------------------------
    struct GroupMsg { QString roomId; QString fromPeerId; qint64 seq; QJsonObject payload; };
    std::vector<GroupMsg> groupMessages;

    // --- buffered messages ---------------------------------------------------
    bool bufferedReceived = false;
    QString bufferedRoomId;
    QJsonArray bufferedMessages;

    // --- ack-confirm ---------------------------------------------------------
    bool ackConfirmReceived = false;
    qint64 ackConfirmSeq = 0;

    // --- errors --------------------------------------------------------------
    std::vector<QString> errors;

    // --- utilities -----------------------------------------------------------
    void resetJoin() {
        joinedReceived = false;
        peerId.clear();
        roomId.clear();
        roomType.clear();
        participants = QJsonArray();
    }

private:
    void wireSignals();
    std::unique_ptr<SignalingService> service_;
};

// ---------------------------------------------------------------------------
// Fixture — starts real server, provides helpers
// ---------------------------------------------------------------------------
class ClientE2eFixture : public ::testing::Test {
protected:
    uint16_t port{};
    static std::atomic<uint16_t> next_port;

    std::unique_ptr<signaling::SignalingServer> server_ptr;
    std::thread server_thrd;

    void SetUp() override;
    void TearDown() override;

    QUrl serverUrl() const;

    // Pump Qt event loop until condition or timeout
    static bool waitFor(const std::function<bool()>& cond, int ms = 3000);

    // Create a TestClient already connected to the server
    std::unique_ptr<TestClient> makeClient();

    // Connect + join, returns ready TestClient (asserts on failure)
    std::unique_ptr<TestClient> makeJoinedClient(const QString& roomId,
                                                  const QString& username,
                                                  const QString& roomType = "direct");
};

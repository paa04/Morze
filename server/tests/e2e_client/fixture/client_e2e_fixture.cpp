#include "fixture/client_e2e_fixture.hpp"

#include <boost/asio.hpp>

// ---------------------------------------------------------------------------
// TestClient signal wiring
// ---------------------------------------------------------------------------
void TestClient::wireSignals() {
    auto* s = service_.get();

    QObject::connect(s, &SignalingService::connected,
        [this] { connected = true; });

    QObject::connect(s, &SignalingService::disconnected,
        [this] { disconnected = true; });

    QObject::connect(s, &SignalingService::joined,
        [this](const QString& rid, const QString& rtype, const QString& pid, const QJsonArray& parts) {
            roomId       = rid;
            roomType     = rtype;
            peerId       = pid;
            participants = parts;
            joinedReceived = true;
        });

    QObject::connect(s, &SignalingService::peerJoined,
        [this](const QString& /*roomId*/, const QString& pid, const QString& uname) {
            peersJoined.push_back({pid, uname});
        });

    QObject::connect(s, &SignalingService::peerLeft,
        [this](const QString& /*roomId*/, const QString& pid) {
            peersLeft.push_back(pid);
        });

    QObject::connect(s, &SignalingService::offerReceived,
        [this](const QString& rid, const QString& from, const QJsonObject& sdp) {
            offersReceived.push_back({rid, from, sdp});
        });

    QObject::connect(s, &SignalingService::answerReceived,
        [this](const QString& rid, const QString& from, const QJsonObject& sdp) {
            answersReceived.push_back({rid, from, sdp});
        });

    QObject::connect(s, &SignalingService::iceCandidateReceived,
        [this](const QString& rid, const QString& from, const QJsonObject& cand) {
            iceCandidatesReceived.push_back({rid, from, cand});
        });

    QObject::connect(s, &SignalingService::groupMessageReceived,
        [this](const QString& rid, const QString& from, qint64 seq, const QJsonObject& pay) {
            groupMessages.push_back({rid, from, seq, pay});
        });

    QObject::connect(s, &SignalingService::bufferedMessagesReceived,
        [this](const QString& rid, const QJsonArray& msgs) {
            bufferedReceived  = true;
            bufferedRoomId    = rid;
            bufferedMessages  = msgs;
        });

    QObject::connect(s, &SignalingService::ackConfirmReceived,
        [this](const QString& /*roomId*/, qint64 seq) {
            ackConfirmReceived = true;
            ackConfirmSeq      = seq;
        });

    QObject::connect(s, &SignalingService::errorReceived,
        [this](const QString& msg) {
            errors.push_back(msg);
        });
}

// ---------------------------------------------------------------------------
// Fixture helpers
// ---------------------------------------------------------------------------
std::atomic<uint16_t> ClientE2eFixture::next_port{20000};

static bool readyCheck(uint16_t port, int timeout_ms = 3000) {
    namespace asio = boost::asio;
    using tcp = asio::ip::tcp;

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

    while (std::chrono::steady_clock::now() < deadline) {
        asio::io_context ioc;
        tcp::socket sock(ioc);
        boost::system::error_code ec;
        sock.connect({asio::ip::make_address("127.0.0.1"), port}, ec);
        if (!ec) {
            boost::system::error_code ignored;
            sock.close(ignored);
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return false;
}

void ClientE2eFixture::SetUp() {
    port = next_port.fetch_add(1);
    server_ptr = std::make_unique<signaling::SignalingServer>(port, 1);
    ASSERT_TRUE(server_ptr->start());
    server_thrd = std::thread([this] { server_ptr->run(); });
    ASSERT_TRUE(readyCheck(port));
}

void ClientE2eFixture::TearDown() {
    if (server_ptr) server_ptr->stop();
    if (server_thrd.joinable()) server_thrd.join();
}

QUrl ClientE2eFixture::serverUrl() const {
    return QUrl(QString("ws://127.0.0.1:%1").arg(port));
}

bool ClientE2eFixture::waitFor(const std::function<bool()>& cond, int ms) {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);

    while (!cond() && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    return cond();
}

std::unique_ptr<TestClient> ClientE2eFixture::makeClient() {
    auto c = std::make_unique<TestClient>();
    (*c)->connectToServer(serverUrl());
    bool ok = waitFor([&] { return c->connected; });
    if (!ok) return nullptr;
    return c;
}

std::unique_ptr<TestClient> ClientE2eFixture::makeJoinedClient(
    const QString& roomId, const QString& username, const QString& roomType)
{
    auto c = makeClient();
    if (!c) return nullptr;
    (*c)->join(roomId, username, roomType);
    bool ok = waitFor([&] { return c->joinedReceived; });
    if (!ok) return nullptr;
    return c;
}

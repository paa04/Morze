#include "fixture/client_e2e_fixture.hpp"

#include "CommunicationController.h"
#include "WebRTCService.h"
#include "ChatDTO.h"
#include "MessageDTO.h"
#include "ChatMemberDTO.h"

namespace {

// Helper: create a connected SignalingService + WebRTCService + Controller
struct ControllerStack {
    std::unique_ptr<SignalingService> signaling;
    std::unique_ptr<WebRTCService>   webrtc;
    std::unique_ptr<CommunicationController> controller;

    explicit ControllerStack(const QUrl& serverUrl) {
        signaling  = std::make_unique<SignalingService>();
        webrtc     = std::make_unique<WebRTCService>();
        controller = std::make_unique<CommunicationController>(
            signaling.get(), webrtc.get());
        signaling->connectToServer(serverUrl);
    }
};

} // namespace

// ===================================================================
// CommunicationController tests
// ===================================================================

TEST_F(ClientE2eFixture, Controller_JoinChatEmitsChatJoined) {
    ControllerStack stack(serverUrl());
    ASSERT_TRUE(waitFor([&] { return stack.signaling->isConnected(); }));

    bool chatJoinedFired = false;
    std::string receivedPeerId;
    std::string receivedRoomType;

    QObject::connect(stack.controller.get(), &CommunicationController::chatJoined,
        [&](const ChatDTO& chat, const std::string& myPeerId) {
            chatJoinedFired = true;
            receivedPeerId  = myPeerId;
            receivedRoomType = chat.getType();
        });

    ChatDTO chat;
    chat.setRoomId("ctrl-room1");
    chat.setType("group");
    stack.controller->joinChat(chat, "alice");

    ASSERT_TRUE(waitFor([&] { return chatJoinedFired; }));
    EXPECT_FALSE(receivedPeerId.empty());
    EXPECT_EQ(receivedRoomType, "group");
}

TEST_F(ClientE2eFixture, Controller_ParticipantJoinedOnPeerJoin) {
    ControllerStack alice(serverUrl());
    ASSERT_TRUE(waitFor([&] { return alice.signaling->isConnected(); }));

    bool participantFired = false;
    ChatMemberDTO receivedMember;

    QObject::connect(alice.controller.get(), &CommunicationController::participantJoined,
        [&](const std::string& /*roomId*/, const ChatMemberDTO& member) {
            participantFired = true;
            receivedMember   = member;
        });

    ChatDTO chat;
    chat.setRoomId("ctrl-room2");
    chat.setType("group");
    alice.controller->joinChat(chat, "alice");

    // Wait for alice's join to complete
    bool aliceJoined = false;
    QObject::connect(alice.controller.get(), &CommunicationController::chatJoined,
        [&](const ChatDTO&, const std::string&) { aliceJoined = true; });
    ASSERT_TRUE(waitFor([&] { return aliceJoined; }));

    // Bob joins
    ControllerStack bob(serverUrl());
    ASSERT_TRUE(waitFor([&] { return bob.signaling->isConnected(); }));

    ChatDTO chat2;
    chat2.setRoomId("ctrl-room2");
    chat2.setType("group");
    bob.controller->joinChat(chat2, "bob");

    ASSERT_TRUE(waitFor([&] { return participantFired; }));
    EXPECT_EQ(receivedMember.getUsername(), "bob");
    EXPECT_FALSE(receivedMember.getId().empty());
}

TEST_F(ClientE2eFixture, Controller_GetParticipantsAfterJoin) {
    ControllerStack alice(serverUrl());
    ASSERT_TRUE(waitFor([&] { return alice.signaling->isConnected(); }));

    ChatDTO chat;
    chat.setRoomId("ctrl-room3");
    chat.setType("group");

    bool aliceJoined = false;
    QObject::connect(alice.controller.get(), &CommunicationController::chatJoined,
        [&](const ChatDTO&, const std::string&) { aliceJoined = true; });
    alice.controller->joinChat(chat, "alice");
    ASSERT_TRUE(waitFor([&] { return aliceJoined; }));

    // Initially no participants (only self, and self is not in participants list)
    auto parts0 = alice.controller->getParticipants("ctrl-room3");
    EXPECT_TRUE(parts0.empty());

    // Bob joins
    ControllerStack bob(serverUrl());
    ASSERT_TRUE(waitFor([&] { return bob.signaling->isConnected(); }));

    bool participantFired = false;
    QObject::connect(alice.controller.get(), &CommunicationController::participantJoined,
        [&](const std::string&, const ChatMemberDTO&) { participantFired = true; });

    bob.controller->joinChat(chat, "bob");
    ASSERT_TRUE(waitFor([&] { return participantFired; }));

    auto parts1 = alice.controller->getParticipants("ctrl-room3");
    ASSERT_EQ(parts1.size(), 1U);
    EXPECT_EQ(parts1[0].getUsername(), "bob");
}

TEST_F(ClientE2eFixture, Controller_GroupMessageRoundTrip) {
    // Alice and Bob both join a group room via controllers
    ControllerStack alice(serverUrl());
    ASSERT_TRUE(waitFor([&] { return alice.signaling->isConnected(); }));

    ControllerStack bob(serverUrl());
    ASSERT_TRUE(waitFor([&] { return bob.signaling->isConnected(); }));

    ChatDTO chat;
    chat.setRoomId("ctrl-room4");
    chat.setType("group");

    bool aliceJoined = false;
    QObject::connect(alice.controller.get(), &CommunicationController::chatJoined,
        [&](const ChatDTO&, const std::string&) { aliceJoined = true; });
    alice.controller->joinChat(chat, "alice");
    ASSERT_TRUE(waitFor([&] { return aliceJoined; }));

    bool bobJoined = false;
    QObject::connect(bob.controller.get(), &CommunicationController::chatJoined,
        [&](const ChatDTO&, const std::string&) { bobJoined = true; });
    bob.controller->joinChat(chat, "bob");
    ASSERT_TRUE(waitFor([&] { return bobJoined; }));

    // Wait for alice to see bob
    bool aliceSeesBob = false;
    QObject::connect(alice.controller.get(), &CommunicationController::participantJoined,
        [&](const std::string&, const ChatMemberDTO&) { aliceSeesBob = true; });
    ASSERT_TRUE(waitFor([&] { return aliceSeesBob; }));

    // Bob listens for incoming messages
    bool bobReceivedMsg = false;
    std::string bobReceivedContent;
    QObject::connect(bob.controller.get(), &CommunicationController::messageReceived,
        [&](const MessageDTO& msg) {
            bobReceivedMsg     = true;
            bobReceivedContent = msg.getContent();
        });

    // Alice sends a group message
    MessageDTO outgoing;
    outgoing.setId("msg-001");
    outgoing.setChatId("ctrl-room4");
    outgoing.setContent("hello from controller");
    outgoing.setDirection("outgoing");

    bool delivered = false;
    QObject::connect(alice.controller.get(), &CommunicationController::messageDelivered,
        [&](const std::string& id) { if (id == "msg-001") delivered = true; });

    alice.controller->sendMessage(outgoing);

    // Alice should get messageDelivered
    ASSERT_TRUE(waitFor([&] { return delivered; }));

    // Bob should get messageReceived via controller
    ASSERT_TRUE(waitFor([&] { return bobReceivedMsg; }));
    EXPECT_EQ(bobReceivedContent, "hello from controller");
}

TEST_F(ClientE2eFixture, Controller_LeaveChatEmitsChatLeft) {
    ControllerStack alice(serverUrl());
    ASSERT_TRUE(waitFor([&] { return alice.signaling->isConnected(); }));

    ChatDTO chat;
    chat.setRoomId("ctrl-room5");
    chat.setType("group");

    bool aliceJoined = false;
    QObject::connect(alice.controller.get(), &CommunicationController::chatJoined,
        [&](const ChatDTO&, const std::string&) { aliceJoined = true; });
    alice.controller->joinChat(chat, "alice");
    ASSERT_TRUE(waitFor([&] { return aliceJoined; }));

    bool chatLeftFired = false;
    std::string leftRoomId;
    QObject::connect(alice.controller.get(), &CommunicationController::chatLeft,
        [&](const std::string& rid) {
            chatLeftFired = true;
            leftRoomId    = rid;
        });

    alice.controller->leaveChat("ctrl-room5");

    ASSERT_TRUE(waitFor([&] { return chatLeftFired; }));
    EXPECT_EQ(leftRoomId, "ctrl-room5");

    // Participants list should be empty after leaving
    auto parts = alice.controller->getParticipants("ctrl-room5");
    EXPECT_TRUE(parts.empty());
}

TEST_F(ClientE2eFixture, Controller_ParticipantLeftOnPeerDisconnect) {
    ControllerStack alice(serverUrl());
    ASSERT_TRUE(waitFor([&] { return alice.signaling->isConnected(); }));

    ChatDTO chat;
    chat.setRoomId("ctrl-room6");
    chat.setType("group");

    bool aliceJoined = false;
    QObject::connect(alice.controller.get(), &CommunicationController::chatJoined,
        [&](const ChatDTO&, const std::string&) { aliceJoined = true; });
    alice.controller->joinChat(chat, "alice");
    ASSERT_TRUE(waitFor([&] { return aliceJoined; }));

    // Bob joins
    auto bobStack = std::make_unique<ControllerStack>(serverUrl());
    ASSERT_TRUE(waitFor([&] { return bobStack->signaling->isConnected(); }));

    bool participantJoined = false;
    QObject::connect(alice.controller.get(), &CommunicationController::participantJoined,
        [&](const std::string&, const ChatMemberDTO&) { participantJoined = true; });

    bobStack->controller->joinChat(chat, "bob");
    ASSERT_TRUE(waitFor([&] { return participantJoined; }));

    // Listen for participant-left
    bool participantLeft = false;
    std::string leftPeerId;
    QObject::connect(alice.controller.get(), &CommunicationController::participantLeft,
        [&](const std::string& /*roomId*/, const std::string& pid) {
            participantLeft = true;
            leftPeerId      = pid;
        });

    // Bob disconnects
    bobStack.reset();

    ASSERT_TRUE(waitFor([&] { return participantLeft; }));
    EXPECT_FALSE(leftPeerId.empty());
}

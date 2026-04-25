#include "fixture/client_e2e_fixture.hpp"

// ===================================================================
// Connection & Join
// ===================================================================

TEST_F(ClientE2eFixture, SignalingService_ConnectsToServer) {
    auto alice = makeClient();
    ASSERT_NE(alice, nullptr);
    EXPECT_TRUE(alice->connected);
}

TEST_F(ClientE2eFixture, SignalingService_JoinDirectRoom) {
    auto alice = makeJoinedClient("room-d1", "alice", "direct");
    ASSERT_NE(alice, nullptr);

    EXPECT_EQ(alice->roomId,   "room-d1");
    EXPECT_EQ(alice->roomType, "direct");
    EXPECT_FALSE(alice->peerId.isEmpty());
    EXPECT_TRUE(alice->participants.isEmpty());
}

TEST_F(ClientE2eFixture, SignalingService_JoinGroupRoom) {
    auto alice = makeJoinedClient("room-g1", "alice", "group");
    ASSERT_NE(alice, nullptr);

    EXPECT_EQ(alice->roomType, "group");
}

TEST_F(ClientE2eFixture, SignalingService_TwoPeersJoinDirectRoom) {
    auto alice = makeJoinedClient("room-d2", "alice", "direct");
    ASSERT_NE(alice, nullptr);

    auto bob = makeJoinedClient("room-d2", "bob", "direct");
    ASSERT_NE(bob, nullptr);

    // Bob should see Alice in participants
    ASSERT_EQ(bob->participants.size(), 1);
    EXPECT_EQ(bob->participants[0].toObject()["username"].toString(), "alice");
    EXPECT_EQ(bob->participants[0].toObject()["peerId"].toString(), alice->peerId);

    // Alice should receive peer-joined for Bob
    ASSERT_TRUE(waitFor([&] { return !alice->peersJoined.empty(); }));
    EXPECT_EQ(alice->peersJoined[0].peerId,   bob->peerId);
    EXPECT_EQ(alice->peersJoined[0].username, "bob");
}

// ===================================================================
// Offer / Answer / ICE forwarding (direct rooms)
// ===================================================================

TEST_F(ClientE2eFixture, SignalingService_OfferIsForwarded) {
    auto alice = makeJoinedClient("room-sig1", "alice");
    ASSERT_NE(alice, nullptr);
    auto bob = makeJoinedClient("room-sig1", "bob");
    ASSERT_NE(bob, nullptr);
    ASSERT_TRUE(waitFor([&] { return !alice->peersJoined.empty(); }));

    QJsonObject sdp{{"type", "offer"}, {"sdp", "v=0"}};
    (*alice)->offer("room-sig1", bob->peerId, sdp);

    ASSERT_TRUE(waitFor([&] { return !bob->offersReceived.empty(); }));
    EXPECT_EQ(bob->offersReceived[0].fromPeerId, alice->peerId);
    EXPECT_EQ(bob->offersReceived[0].payload["sdp"].toString(), "v=0");
}

TEST_F(ClientE2eFixture, SignalingService_AnswerIsForwarded) {
    auto alice = makeJoinedClient("room-sig2", "alice");
    ASSERT_NE(alice, nullptr);
    auto bob = makeJoinedClient("room-sig2", "bob");
    ASSERT_NE(bob, nullptr);
    ASSERT_TRUE(waitFor([&] { return !alice->peersJoined.empty(); }));

    QJsonObject sdp{{"type", "answer"}, {"sdp", "v=0"}};
    (*bob)->answer("room-sig2", alice->peerId, sdp);

    ASSERT_TRUE(waitFor([&] { return !alice->answersReceived.empty(); }));
    EXPECT_EQ(alice->answersReceived[0].fromPeerId, bob->peerId);
    EXPECT_EQ(alice->answersReceived[0].payload["sdp"].toString(), "v=0");
}

TEST_F(ClientE2eFixture, SignalingService_IceCandidateIsForwarded) {
    auto alice = makeJoinedClient("room-sig3", "alice");
    ASSERT_NE(alice, nullptr);
    auto bob = makeJoinedClient("room-sig3", "bob");
    ASSERT_NE(bob, nullptr);
    ASSERT_TRUE(waitFor([&] { return !alice->peersJoined.empty(); }));

    QJsonObject candidate{{"candidate", "candidate:1 ..."}, {"sdpMid", "0"}};
    (*alice)->iceCandidate("room-sig3", bob->peerId, candidate);

    ASSERT_TRUE(waitFor([&] { return !bob->iceCandidatesReceived.empty(); }));
    EXPECT_EQ(bob->iceCandidatesReceived[0].fromPeerId, alice->peerId);
    EXPECT_EQ(bob->iceCandidatesReceived[0].payload["candidate"].toString(), "candidate:1 ...");
}

// ===================================================================
// Leave & Disconnect
// ===================================================================

TEST_F(ClientE2eFixture, SignalingService_LeaveNotifiesPeer) {
    auto alice = makeJoinedClient("room-lv1", "alice");
    ASSERT_NE(alice, nullptr);
    auto bob = makeJoinedClient("room-lv1", "bob");
    ASSERT_NE(bob, nullptr);
    ASSERT_TRUE(waitFor([&] { return !alice->peersJoined.empty(); }));

    (*alice)->leave("room-lv1", alice->peerId);

    ASSERT_TRUE(waitFor([&] { return !bob->peersLeft.empty(); }));
    EXPECT_EQ(bob->peersLeft[0], alice->peerId);
}

TEST_F(ClientE2eFixture, SignalingService_DisconnectNotifiesPeer) {
    auto alice = makeJoinedClient("room-dc1", "alice");
    ASSERT_NE(alice, nullptr);
    auto bob = makeJoinedClient("room-dc1", "bob");
    ASSERT_NE(bob, nullptr);
    ASSERT_TRUE(waitFor([&] { return !alice->peersJoined.empty(); }));

    // Destroy alice's service (simulates crash / network drop)
    alice.reset();

    ASSERT_TRUE(waitFor([&] { return !bob->peersLeft.empty(); }));
}

// ===================================================================
// Group messaging
// ===================================================================

TEST_F(ClientE2eFixture, SignalingService_GroupMessageBroadcast) {
    auto alice = makeJoinedClient("room-gm1", "alice", "group");
    ASSERT_NE(alice, nullptr);
    auto bob = makeJoinedClient("room-gm1", "bob", "group");
    ASSERT_NE(bob, nullptr);
    auto carol = makeJoinedClient("room-gm1", "carol", "group");
    ASSERT_NE(carol, nullptr);

    // Drain peer-joined notifications
    ASSERT_TRUE(waitFor([&] { return alice->peersJoined.size() >= 2; }));

    QJsonObject payload{{"text", "hello group"}};
    (*alice)->groupMessage("room-gm1", payload);

    // Both Bob and Carol should receive the message
    ASSERT_TRUE(waitFor([&] { return !bob->groupMessages.empty(); }));
    ASSERT_TRUE(waitFor([&] { return !carol->groupMessages.empty(); }));

    EXPECT_EQ(bob->groupMessages[0].fromPeerId,              alice->peerId);
    EXPECT_EQ(bob->groupMessages[0].payload["text"].toString(), "hello group");
    EXPECT_GT(bob->groupMessages[0].seq, 0);

    EXPECT_EQ(carol->groupMessages[0].fromPeerId,              alice->peerId);
    EXPECT_EQ(carol->groupMessages[0].payload["text"].toString(), "hello group");
}

TEST_F(ClientE2eFixture, SignalingService_SenderReceivesOwnGroupMessage) {
    auto alice = makeJoinedClient("room-gm2", "alice", "group");
    ASSERT_NE(alice, nullptr);
    auto bob = makeJoinedClient("room-gm2", "bob", "group");
    ASSERT_NE(bob, nullptr);
    ASSERT_TRUE(waitFor([&] { return !alice->peersJoined.empty(); }));

    QJsonObject payload{{"text", "self-echo"}};
    (*alice)->groupMessage("room-gm2", payload);

    // Sender should also receive her own message with the assigned seq
    ASSERT_TRUE(waitFor([&] { return !alice->groupMessages.empty(); }));
    EXPECT_EQ(alice->groupMessages[0].fromPeerId, alice->peerId);
    EXPECT_GT(alice->groupMessages[0].seq, 0);
    EXPECT_EQ(alice->groupMessages[0].payload["text"].toString(), "self-echo");

    // Bob receives it too
    ASSERT_TRUE(waitFor([&] { return !bob->groupMessages.empty(); }));
}

// ===================================================================
// Buffered messages & ACK
// ===================================================================

TEST_F(ClientE2eFixture, SignalingService_BufferedMessagesOnReconnect) {
    auto alice = makeJoinedClient("room-buf", "alice", "group");
    ASSERT_NE(alice, nullptr);
    auto bob   = makeJoinedClient("room-buf", "bob",   "group");
    ASSERT_NE(bob, nullptr);
    auto carol = makeJoinedClient("room-buf", "carol", "group");
    ASSERT_NE(carol, nullptr);

    ASSERT_TRUE(waitFor([&] { return alice->peersJoined.size() >= 2; }));

    // Carol disconnects (member stays in DB)
    carol.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Drain peer-left for carol on alice & bob
    ASSERT_TRUE(waitFor([&] { return !alice->peersLeft.empty(); }));
    ASSERT_TRUE(waitFor([&] { return !bob->peersLeft.empty(); }));

    // Alice sends 2 messages while Carol is offline
    (*alice)->groupMessage("room-buf", QJsonObject{{"text", "msg-one"}});
    (*alice)->groupMessage("room-buf", QJsonObject{{"text", "msg-two"}});

    // Wait for alice & bob to receive live messages
    ASSERT_TRUE(waitFor([&] { return alice->groupMessages.size() >= 2; }));
    ASSERT_TRUE(waitFor([&] { return bob->groupMessages.size() >= 2; }));

    // Carol reconnects
    auto carol2 = makeClient();
    ASSERT_NE(carol2, nullptr);
    (*carol2)->join("room-buf", "carol", "group");
    ASSERT_TRUE(waitFor([&] { return carol2->joinedReceived; }));

    // Carol should receive buffered-messages
    ASSERT_TRUE(waitFor([&] { return carol2->bufferedReceived; }));
    EXPECT_EQ(carol2->bufferedRoomId, "room-buf");
    ASSERT_EQ(carol2->bufferedMessages.size(), 2);
    EXPECT_EQ(carol2->bufferedMessages[0].toObject()["payload"]
                  .toObject()["text"].toString(), "msg-one");
    EXPECT_EQ(carol2->bufferedMessages[1].toObject()["payload"]
                  .toObject()["text"].toString(), "msg-two");
}

TEST_F(ClientE2eFixture, SignalingService_AckConfirmFlow) {
    auto alice = makeJoinedClient("room-ack", "alice", "group");
    ASSERT_NE(alice, nullptr);
    auto bob = makeJoinedClient("room-ack", "bob", "group");
    ASSERT_NE(bob, nullptr);
    ASSERT_TRUE(waitFor([&] { return !alice->peersJoined.empty(); }));

    (*alice)->groupMessage("room-ack", QJsonObject{{"text", "m1"}});

    // Wait for bob to receive the message and get its seq
    ASSERT_TRUE(waitFor([&] { return !bob->groupMessages.empty(); }));
    qint64 seq = bob->groupMessages[0].seq;

    (*bob)->ack("room-ack", seq);

    ASSERT_TRUE(waitFor([&] { return bob->ackConfirmReceived; }));
    EXPECT_EQ(bob->ackConfirmSeq, seq);
}

// ===================================================================
// Error cases
// ===================================================================

TEST_F(ClientE2eFixture, SignalingService_DirectRoomCapacityLimit) {
    auto alice = makeJoinedClient("room-cap", "alice");
    ASSERT_NE(alice, nullptr);
    auto bob = makeJoinedClient("room-cap", "bob");
    ASSERT_NE(bob, nullptr);

    // Third peer should be rejected
    auto carol = makeClient();
    ASSERT_NE(carol, nullptr);
    (*carol)->join("room-cap", "carol", "direct");

    ASSERT_TRUE(waitFor([&] { return !carol->errors.empty(); }));
    EXPECT_TRUE(carol->errors[0].contains("at most 2"));
}

TEST_F(ClientE2eFixture, SignalingService_DuplicateUsernameRejected) {
    auto alice = makeJoinedClient("room-dup", "alice", "group");
    ASSERT_NE(alice, nullptr);

    // Same username while first alice is still online
    auto alice2 = makeClient();
    ASSERT_NE(alice2, nullptr);
    (*alice2)->join("room-dup", "alice", "group");

    ASSERT_TRUE(waitFor([&] { return !alice2->errors.empty(); }));
    EXPECT_TRUE(alice2->errors[0].contains("already in use"));
}

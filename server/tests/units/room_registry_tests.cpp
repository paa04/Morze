#include "domain/room_registry.hpp"
#include "domain/room_store.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

    class FakeConnection : public signaling::domain::IConnection {
    public:
        void sendText(std::string payload) override {
            sent.push_back(std::move(payload));
        }

        std::vector<std::string> sent;
    };

    class FakeRoomStore : public signaling::domain::IRoomStore {
    public:
        std::map<std::string, signaling::domain::RoomRecord> rooms;
        std::map<std::string, signaling::domain::RoomMemberRecord> members;
        std::map<std::string, signaling::domain::PeerSessionRecord> sessions;

        void saveRoom(const signaling::domain::RoomRecord& room) override {
            rooms[room.room_id] = room;
        }
        std::optional<signaling::domain::RoomRecord> findRoom(const std::string& roomId) override {
            auto it = rooms.find(roomId);
            if (it != rooms.end()) return it->second;
            return std::nullopt;
        }
        void removeRoom(const std::string& roomId) override {
            for (auto it = members.begin(); it != members.end();) {
                if (it->second.room_id == roomId) {
                    sessions.erase(it->first);
                    it = members.erase(it);
                } else {
                    ++it;
                }
            }
            rooms.erase(roomId);
        }

        void saveMember(const signaling::domain::RoomMemberRecord& member) override {
            members[member.member_id] = member;
        }
        std::optional<signaling::domain::RoomMemberRecord> findMember(const std::string& memberId) override {
            auto it = members.find(memberId);
            if (it != members.end()) return it->second;
            return std::nullopt;
        }
        std::vector<signaling::domain::RoomMemberRecord> findMembersByRoom(const std::string& roomId) override {
            std::vector<signaling::domain::RoomMemberRecord> result;
            for (const auto& [_, m] : members) {
                if (m.room_id == roomId) result.push_back(m);
            }
            return result;
        }
        void removeMember(const std::string& memberId) override {
            sessions.erase(memberId);
            members.erase(memberId);
        }

        void saveSession(const signaling::domain::PeerSessionRecord& session) override {
            sessions[session.peer_id] = session;
        }
        void removeSession(const std::string& peerId) override {
            sessions.erase(peerId);
        }
        void removeSessionsByRoom(const std::string& roomId) override {
            for (auto it = sessions.begin(); it != sessions.end();) {
                auto mit = members.find(it->second.member_id);
                if (mit != members.end() && mit->second.room_id == roomId) {
                    it = sessions.erase(it);
                } else {
                    ++it;
                }
            }
        }
        void clearAllSessions() override {
            sessions.clear();
        }

        // Group message buffering
        std::vector<signaling::domain::GroupMessageRecord> groupMessages;
        int64_t nextSeq{1};

        int64_t saveGroupMessage(const signaling::domain::GroupMessageRecord& msg) override {
            auto m = msg;
            m.seq = nextSeq++;
            groupMessages.push_back(m);
            return m.seq;
        }
        std::vector<signaling::domain::GroupMessageRecord> getMessagesAfter(
            const std::string& roomId, int64_t afterSeq) override {
            std::vector<signaling::domain::GroupMessageRecord> result;
            for (const auto& m : groupMessages) {
                if (m.room_id == roomId && m.seq > afterSeq) {
                    result.push_back(m);
                }
            }
            return result;
        }
        void updateLastAckedSeq(const std::string& memberId, int64_t seq) override {
            auto it = members.find(memberId);
            if (it != members.end()) {
                it->second.last_acked_msg_seq = seq;
            }
        }
        void cleanupMessages(const std::string& roomId) override {
            int64_t minSeq = std::numeric_limits<int64_t>::max();
            for (const auto& [_, m] : members) {
                if (m.room_id == roomId) {
                    minSeq = std::min(minSeq, m.last_acked_msg_seq);
                }
            }
            if (minSeq <= 0 || minSeq == std::numeric_limits<int64_t>::max()) return;
            groupMessages.erase(
                std::remove_if(groupMessages.begin(), groupMessages.end(),
                    [&](const auto& msg) { return msg.room_id == roomId && msg.seq <= minSeq; }),
                groupMessages.end());
        }
        void removeGroupMessagesByRoom(const std::string& roomId) override {
            groupMessages.erase(
                std::remove_if(groupMessages.begin(), groupMessages.end(),
                    [&](const auto& msg) { return msg.room_id == roomId; }),
                groupMessages.end());
        }
    };

    using signaling::domain::RoomRegistry;
    using signaling::domain::RoomType;

    std::shared_ptr<FakeConnection> makeConn() {
        return std::make_shared<FakeConnection>();
    }

    std::shared_ptr<FakeRoomStore> makeStore() {
        return std::make_shared<FakeRoomStore>();
    }

    // --- Basic join/leave tests ---

    TEST(RoomRegistryTest, JoinGeneratesPeerIdAndReturnsParticipants) {
        auto store = makeStore();
        RoomRegistry registry(store);
        auto c1 = makeConn();
        auto c2 = makeConn();

        auto j1 = registry.join(c1, "room-1", "alice", std::nullopt);
        ASSERT_TRUE(j1.ok);
        EXPECT_EQ(j1.roomId, "room-1");
        EXPECT_EQ(j1.roomType, RoomType::Direct);
        EXPECT_EQ(j1.participants.size(), 0U);
        EXPECT_FALSE(j1.peerId.empty());

        auto j2 = registry.join(c2, "room-1", "bob", std::nullopt);
        ASSERT_TRUE(j2.ok);
        EXPECT_EQ(j2.participants.size(), 1U);
        EXPECT_EQ(j2.participants[0].peerId, j1.peerId);
        EXPECT_EQ(j2.participants[0].username, "alice");
    }

    TEST(RoomRegistryTest, DirectRoomAllowsAtMostTwoParticipants) {
        RoomRegistry registry(makeStore());
        auto c1 = makeConn();
        auto c2 = makeConn();
        auto c3 = makeConn();

        auto j1 = registry.join(c1, "room-1", "alice", RoomType::Direct);
        ASSERT_TRUE(j1.ok);
        auto j2 = registry.join(c2, "room-1", "bob", RoomType::Direct);
        ASSERT_TRUE(j2.ok);

        auto j3 = registry.join(c3, "room-1", "carol", RoomType::Direct);
        EXPECT_FALSE(j3.ok);
        EXPECT_EQ(j3.error, "direct room supports at most 2 participants");
    }

    TEST(RoomRegistryTest, RouteWorksOnlyInsideDirectRoomAndReturnsSenderPeerId) {
        RoomRegistry registry(makeStore());
        auto c1 = makeConn();
        auto c2 = makeConn();

        auto j1 = registry.join(c1, "room-1", "alice", RoomType::Direct);
        auto j2 = registry.join(c2, "room-1", "bob", RoomType::Direct);
        ASSERT_TRUE(j1.ok);
        ASSERT_TRUE(j2.ok);

        std::string error;
        auto route = registry.route(c1, "room-1", j2.peerId, error);
        ASSERT_TRUE(route.has_value());
        EXPECT_TRUE(error.empty());
        EXPECT_EQ(route->fromPeerId, j1.peerId);
        EXPECT_EQ(route->roomId, "room-1");
    }

    TEST(RoomRegistryTest, RouteRejectsGroupRooms) {
        RoomRegistry registry(makeStore());
        auto c1 = makeConn();
        auto c2 = makeConn();

        auto j1 = registry.join(c1, "group-1", "alice", RoomType::Group);
        auto j2 = registry.join(c2, "group-1", "bob", RoomType::Group);
        ASSERT_TRUE(j1.ok);
        ASSERT_TRUE(j2.ok);

        std::string error;
        auto route = registry.route(c1, "group-1", j2.peerId, error);
        EXPECT_FALSE(route.has_value());
        EXPECT_EQ(error, "offer/answer/ice-candidate are allowed only in direct rooms");
    }

    TEST(RoomRegistryTest, LeaveValidatesRoomAndPeerAndNotifiesRemaining) {
        RoomRegistry registry(makeStore());
        auto c1 = makeConn();
        auto c2 = makeConn();

        auto j1 = registry.join(c1, "room-1", "alice", RoomType::Direct);
        auto j2 = registry.join(c2, "room-1", "bob", RoomType::Direct);
        ASSERT_TRUE(j1.ok);
        ASSERT_TRUE(j2.ok);

        std::string error;
        auto badLeave = registry.leave(c1, std::make_optional<std::string>("room-2"), j1.peerId, error);
        EXPECT_FALSE(badLeave.hadMembership);
        EXPECT_EQ(error, "roomId does not match active session room");

        error.clear();
        auto leave = registry.leave(c1, std::make_optional<std::string>("room-1"), j1.peerId, error);
        EXPECT_TRUE(error.empty());
        ASSERT_TRUE(leave.hadMembership);
        EXPECT_EQ(leave.roomId, "room-1");
        EXPECT_EQ(leave.peerId, j1.peerId);
        EXPECT_EQ(leave.peersToNotify.size(), 1U);
    }

    TEST(RoomRegistryTest, LastParticipantLeaveRemovesRoom) {
        auto store = makeStore();
        RoomRegistry registry(store);
        auto c1 = makeConn();

        auto j1 = registry.join(c1, "room-1", "alice", RoomType::Direct);
        ASSERT_TRUE(j1.ok);

        std::string error;
        auto leave = registry.leave(c1, std::make_optional<std::string>("room-1"), j1.peerId, error);
        ASSERT_TRUE(leave.hadMembership);
        EXPECT_TRUE(leave.peersToNotify.empty());

        // Room was removed from DB
        EXPECT_FALSE(store->findRoom("room-1").has_value());

        // New join creates fresh room
        auto c2 = makeConn();
        auto j2 = registry.join(c2, "room-1", "bob", std::nullopt);
        ASSERT_TRUE(j2.ok);
        EXPECT_EQ(j2.participants.size(), 0U);
    }

    // --- Disconnect tests (connection lost, member stays) ---

    TEST(RoomRegistryTest, DisconnectKeepsMemberInRoom) {
        auto store = makeStore();
        RoomRegistry registry(store);
        auto c1 = makeConn();
        auto c2 = makeConn();

        auto j1 = registry.join(c1, "room-1", "alice", RoomType::Direct);
        auto j2 = registry.join(c2, "room-1", "bob", RoomType::Direct);
        ASSERT_TRUE(j1.ok);
        ASSERT_TRUE(j2.ok);

        auto disc = registry.disconnect(c1);
        EXPECT_TRUE(disc.hadMembership);
        EXPECT_EQ(disc.peerId, j1.peerId);
        EXPECT_EQ(disc.peersToNotify.size(), 1U);

        // Alice is offline but still a member in DB
        EXPECT_TRUE(store->findMember(j1.peerId).has_value());

        // 3rd person can't join Direct room — still 2 members
        auto c3 = makeConn();
        auto j3 = registry.join(c3, "room-1", "carol", RoomType::Direct);
        EXPECT_FALSE(j3.ok);
        EXPECT_EQ(j3.error, "direct room supports at most 2 participants");
    }

    TEST(RoomRegistryTest, DisconnectDoesNotRemoveRoom) {
        auto store = makeStore();
        RoomRegistry registry(store);
        auto c1 = makeConn();

        auto j1 = registry.join(c1, "room-1", "alice", RoomType::Group);
        ASSERT_TRUE(j1.ok);

        registry.disconnect(c1);

        // Room and member still in DB
        EXPECT_TRUE(store->findRoom("room-1").has_value());
        EXPECT_TRUE(store->findMember(j1.peerId).has_value());

        // Reconnect should work
        auto c2 = makeConn();
        auto j2 = registry.join(c2, "room-1", "alice", RoomType::Group);
        ASSERT_TRUE(j2.ok);
        EXPECT_EQ(j2.peerId, j1.peerId);
    }

    // --- Reconnect tests ---

    TEST(RoomRegistryTest, ReconnectReusesPeerId) {
        RoomRegistry registry(makeStore());
        auto c1 = makeConn();

        auto j1 = registry.join(c1, "room-1", "alice", RoomType::Group);
        ASSERT_TRUE(j1.ok);
        auto originalPeerId = j1.peerId;

        registry.disconnect(c1);

        auto c2 = makeConn();
        auto j2 = registry.join(c2, "room-1", "alice", RoomType::Group);
        ASSERT_TRUE(j2.ok);
        EXPECT_EQ(j2.peerId, originalPeerId);
    }

    TEST(RoomRegistryTest, ReconnectNotifiesOnlinePeers) {
        RoomRegistry registry(makeStore());
        auto c1 = makeConn();
        auto c2 = makeConn();

        auto j1 = registry.join(c1, "room-1", "alice", RoomType::Group);
        auto j2 = registry.join(c2, "room-1", "bob", RoomType::Group);
        ASSERT_TRUE(j1.ok);
        ASSERT_TRUE(j2.ok);

        registry.disconnect(c1);

        auto c3 = makeConn();
        auto j3 = registry.join(c3, "room-1", "alice", RoomType::Group);
        ASSERT_TRUE(j3.ok);
        EXPECT_EQ(j3.peersToNotify.size(), 1U);
        EXPECT_EQ(j3.participants.size(), 1U);
        EXPECT_EQ(j3.participants[0].username, "bob");
    }

    TEST(RoomRegistryTest, ReconnectRejectsIfAlreadyOnline) {
        RoomRegistry registry(makeStore());
        auto c1 = makeConn();

        auto j1 = registry.join(c1, "room-1", "alice", RoomType::Group);
        ASSERT_TRUE(j1.ok);

        auto c2 = makeConn();
        auto j2 = registry.join(c2, "room-1", "alice", RoomType::Group);
        EXPECT_FALSE(j2.ok);
        EXPECT_EQ(j2.error, "username already in use in this room");
    }

    TEST(RoomRegistryTest, ReconnectAllowedInDirectRoom) {
        RoomRegistry registry(makeStore());
        auto c1 = makeConn();
        auto c2 = makeConn();

        auto j1 = registry.join(c1, "room-1", "alice", RoomType::Direct);
        auto j2 = registry.join(c2, "room-1", "bob", RoomType::Direct);
        ASSERT_TRUE(j1.ok);
        ASSERT_TRUE(j2.ok);

        registry.disconnect(c1);

        auto c3 = makeConn();
        auto j3 = registry.join(c3, "room-1", "alice", RoomType::Direct);
        ASSERT_TRUE(j3.ok);
        EXPECT_EQ(j3.peerId, j1.peerId);
    }

    // --- DB state verification tests ---

    TEST(RoomRegistryTest, JoinPersistsRoomAndMember) {
        auto store = makeStore();
        RoomRegistry registry(store);
        auto c1 = makeConn();

        auto j1 = registry.join(c1, "room-1", "alice", RoomType::Direct);
        ASSERT_TRUE(j1.ok);

        EXPECT_TRUE(store->findRoom("room-1").has_value());
        EXPECT_EQ(store->rooms["room-1"].type, "direct");

        EXPECT_TRUE(store->findMember(j1.peerId).has_value());
        EXPECT_EQ(store->members[j1.peerId].username, "alice");

        // Direct rooms don't track peer sessions
        EXPECT_TRUE(store->sessions.empty());
    }

    TEST(RoomRegistryTest, GroupJoinCreatesPeerSession) {
        auto store = makeStore();
        RoomRegistry registry(store);
        auto c1 = makeConn();

        auto j1 = registry.join(c1, "room-1", "alice", RoomType::Group);
        ASSERT_TRUE(j1.ok);

        EXPECT_EQ(store->sessions.size(), 1U);
        EXPECT_EQ(store->sessions[j1.peerId].connection_state, "connected");
    }

    TEST(RoomRegistryTest, DisconnectRemovesSessionKeepsMember) {
        auto store = makeStore();
        RoomRegistry registry(store);
        auto c1 = makeConn();

        auto j1 = registry.join(c1, "room-1", "alice", RoomType::Group);
        ASSERT_TRUE(j1.ok);

        registry.disconnect(c1);

        EXPECT_TRUE(store->sessions.empty());
        EXPECT_TRUE(store->findMember(j1.peerId).has_value());
        EXPECT_TRUE(store->findRoom("room-1").has_value());
    }

    TEST(RoomRegistryTest, LeaveCleansUpMemberFromDB) {
        auto store = makeStore();
        RoomRegistry registry(store);
        auto c1 = makeConn();
        auto c2 = makeConn();

        auto j1 = registry.join(c1, "room-1", "alice", RoomType::Direct);
        auto j2 = registry.join(c2, "room-1", "bob", RoomType::Direct);
        ASSERT_TRUE(j1.ok);
        ASSERT_TRUE(j2.ok);

        std::string error;
        registry.leave(c1, std::make_optional<std::string>("room-1"), j1.peerId, error);

        EXPECT_TRUE(store->findRoom("room-1").has_value());
        EXPECT_FALSE(store->findMember(j1.peerId).has_value());
        EXPECT_TRUE(store->findMember(j2.peerId).has_value());
    }

    TEST(RoomRegistryTest, LastLeaveRemovesRoomFromDB) {
        auto store = makeStore();
        RoomRegistry registry(store);
        auto c1 = makeConn();

        auto j1 = registry.join(c1, "room-1", "alice", RoomType::Direct);
        ASSERT_TRUE(j1.ok);

        std::string error;
        registry.leave(c1, std::make_optional<std::string>("room-1"), j1.peerId, error);

        EXPECT_TRUE(store->rooms.empty());
        EXPECT_TRUE(store->members.empty());
    }

    // --- Group message buffering tests ---

    TEST(RoomRegistryTest, BroadcastSavesMessageAndReturnsSeq) {
        auto store = makeStore();
        RoomRegistry registry(store);
        auto c1 = makeConn();
        auto c2 = makeConn();

        registry.join(c1, "room-1", "alice", RoomType::Group);
        registry.join(c2, "room-1", "bob", RoomType::Group);

        std::string err;
        auto res = registry.broadcast(c1, "room-1", R"({"text":"hello"})", err);
        ASSERT_TRUE(res.has_value());
        EXPECT_GT(res->messageSeq, 0);
        EXPECT_EQ(store->groupMessages.size(), 1U);
        EXPECT_EQ(store->groupMessages[0].payload, R"({"text":"hello"})");
    }

    TEST(RoomRegistryTest, GetPendingMessagesForOfflineMember) {
        auto store = makeStore();
        RoomRegistry registry(store);
        auto c1 = makeConn();
        auto c2 = makeConn();

        auto j1 = registry.join(c1, "room-1", "alice", RoomType::Group);
        auto j2 = registry.join(c2, "room-1", "bob", RoomType::Group);

        // Bob goes offline
        registry.disconnect(c2);

        // Alice sends a message
        std::string err;
        registry.broadcast(c1, "room-1", R"({"text":"missed"})", err);

        // Bob's pending messages
        auto pending = registry.getPendingMessages(j2.peerId);
        EXPECT_EQ(pending.size(), 1U);
        EXPECT_EQ(pending[0].payload, R"({"text":"missed"})");
    }

    TEST(RoomRegistryTest, AckUpdatesCursorAndCleansUp) {
        auto store = makeStore();
        RoomRegistry registry(store);
        auto c1 = makeConn();
        auto c2 = makeConn();

        auto j1 = registry.join(c1, "room-1", "alice", RoomType::Group);
        auto j2 = registry.join(c2, "room-1", "bob", RoomType::Group);

        std::string err;
        auto res = registry.broadcast(c1, "room-1", R"({"text":"hello"})", err);
        ASSERT_TRUE(res.has_value());

        // Both ACK
        registry.acknowledgeMessages(c1, "room-1", res->messageSeq, err);
        registry.acknowledgeMessages(c2, "room-1", res->messageSeq, err);

        // Message cleaned up
        EXPECT_TRUE(store->groupMessages.empty());

        // No pending messages
        auto pending = registry.getPendingMessages(j2.peerId);
        EXPECT_TRUE(pending.empty());
    }

    TEST(RoomRegistryTest, CleanupBlockedByLowestCursor) {
        auto store = makeStore();
        RoomRegistry registry(store);
        auto c1 = makeConn();
        auto c2 = makeConn();

        registry.join(c1, "room-1", "alice", RoomType::Group);
        registry.join(c2, "room-1", "bob", RoomType::Group);

        std::string err;
        auto r1 = registry.broadcast(c1, "room-1", R"({"n":1})", err);
        auto r2 = registry.broadcast(c1, "room-1", R"({"n":2})", err);

        // Only alice ACKs both
        registry.acknowledgeMessages(c1, "room-1", r2->messageSeq, err);

        // Bob hasn't ACKed — messages stay
        EXPECT_EQ(store->groupMessages.size(), 2U);
    }

    TEST(RoomRegistryTest, LeaveTriggersCleanup) {
        auto store = makeStore();
        RoomRegistry registry(store);
        auto c1 = makeConn();
        auto c2 = makeConn();

        auto j1 = registry.join(c1, "room-1", "alice", RoomType::Group);
        auto j2 = registry.join(c2, "room-1", "bob", RoomType::Group);

        std::string err;
        auto res = registry.broadcast(c1, "room-1", R"({"text":"bye"})", err);

        // Alice ACKs
        registry.acknowledgeMessages(c1, "room-1", res->messageSeq, err);

        // Bob leaves without ACKing — his cursor no longer blocks
        registry.leave(c2, std::make_optional<std::string>("room-1"), j2.peerId, err);

        // Message should be cleaned up
        EXPECT_TRUE(store->groupMessages.empty());
    }

} // namespace

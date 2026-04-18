#include "infrastructure/persistence/sqlite_room_store.hpp"

#include <gtest/gtest.h>

namespace {

using signaling::domain::RoomRecord;
using signaling::domain::RoomMemberRecord;
using signaling::infrastructure::SqliteRoomStore;

class SqliteRoomStoreTest : public ::testing::Test {
protected:
    SqliteRoomStore store{":memory:"};
};

// --- Room tests ---

TEST_F(SqliteRoomStoreTest, SaveAndFindRoom) {
    RoomRecord room{"r1", "direct", "Room 1", "", "2026-01-01T00:00:00Z"};
    store.saveRoom(room);

    auto found = store.findRoom("r1");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->room_id, "r1");
    EXPECT_EQ(found->type, "direct");
    EXPECT_EQ(found->name, "Room 1");
    EXPECT_EQ(found->created_at, "2026-01-01T00:00:00Z");
}

TEST_F(SqliteRoomStoreTest, FindRoomNotFound) {
    auto found = store.findRoom("nonexistent");
    EXPECT_FALSE(found.has_value());
}

TEST_F(SqliteRoomStoreTest, RemoveRoom) {
    store.saveRoom({"r1", "direct", "Room 1", "", "2026-01-01T00:00:00Z"});
    store.removeRoom("r1");

    EXPECT_FALSE(store.findRoom("r1").has_value());
}

TEST_F(SqliteRoomStoreTest, SaveRoomReplace) {
    store.saveRoom({"r1", "direct", "Room 1", "", "2026-01-01T00:00:00Z"});
    store.saveRoom({"r1", "group", "Updated Room", "suffix", "2026-02-01T00:00:00Z"});

    auto found = store.findRoom("r1");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->type, "group");
    EXPECT_EQ(found->name, "Updated Room");
}

// --- Member tests ---

TEST_F(SqliteRoomStoreTest, SaveAndFindMember) {
    store.saveRoom({"r1", "direct", "Room 1", "", "2026-01-01T00:00:00Z"});
    store.saveMember({"peer1", "alice", "2026-01-01T00:00:00Z", "r1"});

    auto found = store.findMember("peer1");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->member_id, "peer1");
    EXPECT_EQ(found->username, "alice");
    EXPECT_EQ(found->room_id, "r1");
}

TEST_F(SqliteRoomStoreTest, FindMembersByRoom) {
    store.saveRoom({"r1", "group", "Room 1", "", "2026-01-01T00:00:00Z"});
    store.saveMember({"p1", "alice", "2026-01-01T00:00:00Z", "r1"});
    store.saveMember({"p2", "bob", "2026-01-01T00:00:00Z", "r1"});
    store.saveMember({"p3", "carol", "2026-01-01T00:00:00Z", "r1"});

    auto members = store.findMembersByRoom("r1");
    EXPECT_EQ(members.size(), 3U);
}

TEST_F(SqliteRoomStoreTest, FindMembersByRoomFiltersOtherRooms) {
    store.saveRoom({"r1", "direct", "Room 1", "", "2026-01-01T00:00:00Z"});
    store.saveRoom({"r2", "direct", "Room 2", "", "2026-01-01T00:00:00Z"});
    store.saveMember({"p1", "alice", "2026-01-01T00:00:00Z", "r1"});
    store.saveMember({"p2", "bob", "2026-01-01T00:00:00Z", "r2"});

    auto members = store.findMembersByRoom("r1");
    EXPECT_EQ(members.size(), 1U);
    EXPECT_EQ(members[0].username, "alice");
}

TEST_F(SqliteRoomStoreTest, RemoveMember) {
    store.saveRoom({"r1", "direct", "Room 1", "", "2026-01-01T00:00:00Z"});
    store.saveMember({"p1", "alice", "2026-01-01T00:00:00Z", "r1"});
    store.removeMember("p1");

    EXPECT_FALSE(store.findMember("p1").has_value());
}

// --- PeerSession tests ---

TEST_F(SqliteRoomStoreTest, RemoveRoomCascadesMembers) {
    store.saveRoom({"r1", "group", "Room 1", "", "2026-01-01T00:00:00Z"});
    store.saveMember({"p1", "alice", "2026-01-01T00:00:00Z", "r1"});
    store.saveMember({"p2", "bob", "2026-01-01T00:00:00Z", "r1"});

    store.removeRoom("r1");

    EXPECT_FALSE(store.findRoom("r1").has_value());
    EXPECT_FALSE(store.findMember("p1").has_value());
    EXPECT_FALSE(store.findMember("p2").has_value());
    EXPECT_TRUE(store.findMembersByRoom("r1").empty());
}

// --- Group message tests ---

TEST_F(SqliteRoomStoreTest, SaveAndGetGroupMessages) {
    store.saveRoom({"r1", "group", "Room 1", "", "2026-01-01T00:00:00Z"});

    auto s1 = store.saveGroupMessage({0, "r1", "p1", R"({"n":1})", "2026-01-01T00:00:01Z"});
    auto s2 = store.saveGroupMessage({0, "r1", "p1", R"({"n":2})", "2026-01-01T00:00:02Z"});
    auto s3 = store.saveGroupMessage({0, "r1", "p1", R"({"n":3})", "2026-01-01T00:00:03Z"});

    EXPECT_GT(s1, 0);
    EXPECT_EQ(s2, s1 + 1);
    EXPECT_EQ(s3, s2 + 1);

    auto msgs = store.getMessagesAfter("r1", s1);
    EXPECT_EQ(msgs.size(), 2U);
    EXPECT_EQ(msgs[0].seq, s2);
    EXPECT_EQ(msgs[1].seq, s3);
}

TEST_F(SqliteRoomStoreTest, UpdateLastAckedSeq) {
    store.saveRoom({"r1", "group", "Room 1", "", "2026-01-01T00:00:00Z"});
    store.saveMember({"p1", "alice", "2026-01-01T00:00:00Z", "r1", 0});

    store.updateLastAckedSeq("p1", 42);

    auto member = store.findMember("p1");
    ASSERT_TRUE(member.has_value());
    EXPECT_EQ(member->last_acked_msg_seq, 42);
}

TEST_F(SqliteRoomStoreTest, CleanupMessagesDeletesUpToMinCursor) {
    store.saveRoom({"r1", "group", "Room 1", "", "2026-01-01T00:00:00Z"});
    store.saveMember({"p1", "alice", "2026-01-01T00:00:00Z", "r1", 0});
    store.saveMember({"p2", "bob", "2026-01-01T00:00:00Z", "r1", 0});

    auto s1 = store.saveGroupMessage({0, "r1", "p1", R"({"n":1})", "2026-01-01T00:00:01Z"});
    auto s2 = store.saveGroupMessage({0, "r1", "p1", R"({"n":2})", "2026-01-01T00:00:02Z"});
    store.saveGroupMessage({0, "r1", "p1", R"({"n":3})", "2026-01-01T00:00:03Z"});

    // Alice acked up to s2, Bob only up to s1
    store.updateLastAckedSeq("p1", s2);
    store.updateLastAckedSeq("p2", s1);

    store.cleanupMessages("r1");

    // Only msg s1 should be deleted (min cursor = s1)
    auto remaining = store.getMessagesAfter("r1", 0);
    EXPECT_EQ(remaining.size(), 2U);
    EXPECT_EQ(remaining[0].seq, s2);
}

TEST_F(SqliteRoomStoreTest, RemoveGroupMessagesByRoom) {
    store.saveRoom({"r1", "group", "Room 1", "", "2026-01-01T00:00:00Z"});
    store.saveGroupMessage({0, "r1", "p1", R"({"n":1})", "2026-01-01T00:00:01Z"});
    store.saveGroupMessage({0, "r1", "p1", R"({"n":2})", "2026-01-01T00:00:02Z"});

    store.removeGroupMessagesByRoom("r1");

    auto msgs = store.getMessagesAfter("r1", 0);
    EXPECT_TRUE(msgs.empty());
}

} // namespace

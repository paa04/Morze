#include "infrastructure/persistence/sqlite_room_store.hpp"

#include <gtest/gtest.h>

namespace {

using signaling::domain::RoomRecord;
using signaling::domain::RoomMemberRecord;
using signaling::domain::PeerSessionRecord;
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

TEST_F(SqliteRoomStoreTest, SaveAndRemoveSession) {
    store.saveRoom({"r1", "direct", "Room 1", "", "2026-01-01T00:00:00Z"});
    store.saveMember({"p1", "alice", "2026-01-01T00:00:00Z", "r1"});
    store.saveSession({"p1", "connected", "p1"});

    // Session exists — remove it
    store.removeSession("p1");
    // No crash on double remove
    store.removeSession("p1");
}

TEST_F(SqliteRoomStoreTest, ClearAllSessions) {
    store.saveRoom({"r1", "group", "Room 1", "", "2026-01-01T00:00:00Z"});
    store.saveMember({"p1", "alice", "2026-01-01T00:00:00Z", "r1"});
    store.saveMember({"p2", "bob", "2026-01-01T00:00:00Z", "r1"});
    store.saveSession({"p1", "connected", "p1"});
    store.saveSession({"p2", "connected", "p2"});

    store.clearAllSessions();

    // Members and room still exist
    EXPECT_TRUE(store.findRoom("r1").has_value());
    EXPECT_TRUE(store.findMember("p1").has_value());
    EXPECT_TRUE(store.findMember("p2").has_value());
}

TEST_F(SqliteRoomStoreTest, RemoveRoomCascadesMembers) {
    store.saveRoom({"r1", "group", "Room 1", "", "2026-01-01T00:00:00Z"});
    store.saveMember({"p1", "alice", "2026-01-01T00:00:00Z", "r1"});
    store.saveMember({"p2", "bob", "2026-01-01T00:00:00Z", "r1"});
    store.saveSession({"p1", "connected", "p1"});

    store.removeRoom("r1");

    EXPECT_FALSE(store.findRoom("r1").has_value());
    EXPECT_FALSE(store.findMember("p1").has_value());
    EXPECT_FALSE(store.findMember("p2").has_value());
    EXPECT_TRUE(store.findMembersByRoom("r1").empty());
}

} // namespace

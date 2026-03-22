#include "domain/room_registry.hpp"

#include <gtest/gtest.h>

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

using signaling::domain::RoomRegistry;
using signaling::domain::RoomType;

std::shared_ptr<FakeConnection> makeConn() {
  return std::make_shared<FakeConnection>();
}

TEST(RoomRegistryTest, JoinGeneratesPeerIdAndReturnsParticipants) {
  RoomRegistry registry;
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
  RoomRegistry registry;
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
  RoomRegistry registry;
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
  RoomRegistry registry;
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
  RoomRegistry registry;
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
  RoomRegistry registry;
  auto c1 = makeConn();

  auto j1 = registry.join(c1, "room-1", "alice", RoomType::Direct);
  ASSERT_TRUE(j1.ok);

  std::string error;
  auto leave = registry.leave(c1, std::make_optional<std::string>("room-1"), j1.peerId, error);
  ASSERT_TRUE(leave.hadMembership);
  EXPECT_TRUE(leave.peersToNotify.empty());

  auto c2 = makeConn();
  auto j2 = registry.join(c2, "room-1", "bob", std::nullopt);
  ASSERT_TRUE(j2.ok);
  EXPECT_EQ(j2.participants.size(), 0U);
}

} // namespace

#include "signaling_server.hpp"

#include <boost/json.hpp>
#include <gtest/gtest.h>

#include <string>

namespace {

namespace json = boost::json;

TEST(ProtocolTest, ParseObjectAcceptsJsonObject) {
  std::string error;
  auto obj = signaling::protocol::parseObject(
      R"({"type":"join","room":"r1","id":"p1","room_type":"direct"})", error);

  ASSERT_TRUE(obj.has_value());
  EXPECT_TRUE(error.empty());

  auto type = signaling::protocol::getStringField(*obj, "type");
  ASSERT_TRUE(type.has_value());
  EXPECT_EQ(*type, "join");
}

TEST(ProtocolTest, ParseObjectRejectsInvalidJson) {
  std::string error;
  auto obj = signaling::protocol::parseObject("{bad json}", error);

  EXPECT_FALSE(obj.has_value());
  EXPECT_FALSE(error.empty());
}

TEST(ProtocolTest, ParseObjectRejectsNonObjectRoot) {
  std::string error;
  auto obj = signaling::protocol::parseObject(R"([1,2,3])", error);

  EXPECT_FALSE(obj.has_value());
  EXPECT_EQ(error, "json payload must be an object");
}

TEST(ProtocolTest, GetStringFieldReturnsNulloptForMissingOrWrongType) {
  json::object obj;
  obj["name"] = "alice";
  obj["num"] = 42;

  auto name = signaling::protocol::getStringField(obj, "name");
  auto missing = signaling::protocol::getStringField(obj, "missing");
  auto wrong = signaling::protocol::getStringField(obj, "num");

  ASSERT_TRUE(name.has_value());
  EXPECT_EQ(*name, "alice");
  EXPECT_FALSE(missing.has_value());
  EXPECT_FALSE(wrong.has_value());
}

TEST(ProtocolTest, MessageBuildersProduceExpectedShape) {
  auto err = signaling::protocol::makeError("boom");
  EXPECT_EQ(err.at("type").as_string(), "error");
  EXPECT_EQ(err.at("message").as_string(), "boom");

  auto peers = signaling::protocol::makePeers({"a", "b"});
  EXPECT_EQ(peers.at("type").as_string(), "peers");
  ASSERT_TRUE(peers.at("peers").is_array());
  EXPECT_EQ(peers.at("peers").as_array().size(), 2U);

  auto joined = signaling::protocol::makePeerJoined("new-peer");
  EXPECT_EQ(joined.at("type").as_string(), "peer-joined");
  EXPECT_EQ(joined.at("id").as_string(), "new-peer");

  auto left = signaling::protocol::makePeerLeft("old-peer");
  EXPECT_EQ(left.at("type").as_string(), "peer-left");
  EXPECT_EQ(left.at("id").as_string(), "old-peer");

  auto relay = signaling::protocol::makeRelayForward("from-1", json::value("payload"));
  EXPECT_EQ(relay.at("type").as_string(), "relay");
  EXPECT_EQ(relay.at("from").as_string(), "from-1");
  EXPECT_EQ(relay.at("data").as_string(), "payload");
}

TEST(ProtocolTest, MakeSignalForwardPreservesDataValue) {
  json::object data;
  data["sdp"] = "offer";
  data["candidate"] = "cand-1";

  auto signal = signaling::protocol::makeSignalForward("peer-x", data);

  EXPECT_EQ(signal.at("type").as_string(), "signal");
  EXPECT_EQ(signal.at("from").as_string(), "peer-x");
  ASSERT_TRUE(signal.at("data").is_object());
  EXPECT_EQ(signal.at("data").as_object().at("sdp").as_string(), "offer");
}

} // namespace

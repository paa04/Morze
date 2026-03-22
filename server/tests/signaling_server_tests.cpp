#include "application/protocol.hpp"

#include <boost/json.hpp>
#include <gtest/gtest.h>

#include <string>

namespace {

namespace json = boost::json;

TEST(ProtocolTest, ParseObjectAcceptsJsonObject) {
  std::string error;
  auto obj = signaling::application::protocol::parseObject(
      R"({"type":"join","roomId":"r1","username":"alice"})", error);

  ASSERT_TRUE(obj.has_value());
  EXPECT_TRUE(error.empty());

  auto type = signaling::application::protocol::getStringField(*obj, "type");
  ASSERT_TRUE(type.has_value());
  EXPECT_EQ(*type, "join");
}

TEST(ProtocolTest, ParseObjectRejectsInvalidJson) {
  std::string error;
  auto obj = signaling::application::protocol::parseObject("{bad json}", error);

  EXPECT_FALSE(obj.has_value());
  EXPECT_FALSE(error.empty());
}

TEST(ProtocolTest, ParseObjectRejectsNonObjectRoot) {
  std::string error;
  auto obj = signaling::application::protocol::parseObject(R"([1,2,3])", error);

  EXPECT_FALSE(obj.has_value());
  EXPECT_EQ(error, "json payload must be an object");
}

TEST(ProtocolTest, GetStringFieldReturnsNulloptForMissingOrWrongType) {
  json::object obj;
  obj["name"] = "alice";
  obj["num"] = 42;

  auto name = signaling::application::protocol::getStringField(obj, "name");
  auto missing = signaling::application::protocol::getStringField(obj, "missing");
  auto wrong = signaling::application::protocol::getStringField(obj, "num");

  ASSERT_TRUE(name.has_value());
  EXPECT_EQ(*name, "alice");
  EXPECT_FALSE(missing.has_value());
  EXPECT_FALSE(wrong.has_value());
}

TEST(ProtocolTest, MessageBuildersProduceExpectedShape) {
  auto err = signaling::application::protocol::makeError("boom");
  EXPECT_EQ(err.at("type").as_string(), "error");
  EXPECT_EQ(err.at("message").as_string(), "boom");

  std::vector<signaling::domain::Participant> participants{
      {"peer2", "bob"},
      {"peer3", "carol"},
  };
  auto joined = signaling::application::protocol::makeJoined("R1", "direct", "peer1", participants);
  EXPECT_EQ(joined.at("type").as_string(), "joined");
  EXPECT_EQ(joined.at("roomId").as_string(), "R1");
  EXPECT_EQ(joined.at("roomType").as_string(), "direct");
  EXPECT_EQ(joined.at("peerId").as_string(), "peer1");
  ASSERT_TRUE(joined.at("participants").is_array());
  EXPECT_EQ(joined.at("participants").as_array().size(), 2U);

  auto peerJoined = signaling::application::protocol::makePeerJoined("R1", "peer4", "dave");
  EXPECT_EQ(peerJoined.at("type").as_string(), "peer-joined");
  EXPECT_EQ(peerJoined.at("roomId").as_string(), "R1");
  EXPECT_EQ(peerJoined.at("peerId").as_string(), "peer4");
  EXPECT_EQ(peerJoined.at("username").as_string(), "dave");

  auto left = signaling::application::protocol::makePeerLeft("R1", "peer5");
  EXPECT_EQ(left.at("type").as_string(), "peer-left");
  EXPECT_EQ(left.at("roomId").as_string(), "R1");
  EXPECT_EQ(left.at("peerId").as_string(), "peer5");

  auto offer = signaling::application::protocol::makeOffer("R1", "peer1", "peer2", json::value("v=0..."));
  EXPECT_EQ(offer.at("type").as_string(), "offer");
  EXPECT_EQ(offer.at("fromPeerId").as_string(), "peer1");
  EXPECT_EQ(offer.at("toPeerId").as_string(), "peer2");

  auto answer = signaling::application::protocol::makeAnswer("R1", "peer2", "peer1", json::value("v=0..."));
  EXPECT_EQ(answer.at("type").as_string(), "answer");

  json::object candidate;
  candidate["candidate"] = "candidate:1";
  auto ice = signaling::application::protocol::makeIceCandidate("R1", "peer1", "peer2", candidate);
  EXPECT_EQ(ice.at("type").as_string(), "ice-candidate");
  ASSERT_TRUE(ice.at("candidate").is_object());
}

TEST(ProtocolTest, MakeSignalForwardPreservesDataValue) {
  json::object sdp;
  sdp["type"] = "offer";
  sdp["sdp"] = "v=0...";

  auto offer = signaling::application::protocol::makeOffer("R1", "peer-x", "peer-y", sdp);

  EXPECT_EQ(offer.at("type").as_string(), "offer");
  EXPECT_EQ(offer.at("fromPeerId").as_string(), "peer-x");
  ASSERT_TRUE(offer.at("sdp").is_object());
  EXPECT_EQ(offer.at("sdp").as_object().at("type").as_string(), "offer");
}

} // namespace

#include "signaling_server.hpp"

#include <gtest/gtest.h>

#include <array>
#include <string>

namespace {

std::string makeMaskedTextFrame(const std::string& text) {
  std::string frame;
  frame.push_back(static_cast<char>(0x81));
  frame.push_back(static_cast<char>(0x80 | static_cast<uint8_t>(text.size())));

  constexpr std::array<uint8_t, 4> mask = {0x11, 0x22, 0x33, 0x44};
  frame.append(reinterpret_cast<const char*>(mask.data()), mask.size());

  for (size_t i = 0; i < text.size(); ++i) {
    frame.push_back(static_cast<char>(static_cast<uint8_t>(text[i]) ^ mask[i % mask.size()]));
  }

  return frame;
}

TEST(ProtocolTest, WebSocketAcceptMatchesRfcVector) {
  const std::string key = "dGhlIHNhbXBsZSBub25jZQ==";
  const std::string expected = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";
  EXPECT_EQ(signaling::protocol::wsAcceptValue(key), expected);
}

TEST(ProtocolTest, JsonFieldParsingWorksForStringAndRawObject) {
  const std::string payload =
      R"({"type":"signal","to":"peer-1","data":{"sdp":"offer","arr":[1,2,3]}})";

  auto type = signaling::protocol::getJsonStringField(payload, "type");
  auto to = signaling::protocol::getJsonStringField(payload, "to");
  auto rawData = signaling::protocol::getJsonRawValue(payload, "data");

  ASSERT_TRUE(type.has_value());
  ASSERT_TRUE(to.has_value());
  ASSERT_TRUE(rawData.has_value());

  EXPECT_EQ(*type, "signal");
  EXPECT_EQ(*to, "peer-1");
  EXPECT_NE(rawData->find("\"sdp\":\"offer\""), std::string::npos);
  EXPECT_NE(rawData->find("\"arr\":[1,2,3]"), std::string::npos);
}

TEST(ProtocolTest, JsonEscapeEscapesControlAndQuotes) {
  const std::string input = "line\n\"quoted\"\\slash";
  const std::string escaped = signaling::protocol::jsonEscape(input);

  EXPECT_EQ(escaped, "line\\n\\\"quoted\\\"\\\\slash");
}

TEST(ProtocolTest, DecodeClientFrameReturnsPayload) {
  std::string buffer = makeMaskedTextFrame("hello");
  std::string payload;
  uint8_t opcode = 0;
  bool isClose = false;

  const bool ok = signaling::protocol::decodeClientFrame(buffer, payload, opcode, isClose);

  ASSERT_TRUE(ok);
  EXPECT_EQ(opcode, 0x01);
  EXPECT_FALSE(isClose);
  EXPECT_EQ(payload, "hello");
  EXPECT_TRUE(buffer.empty());
}

TEST(ProtocolTest, DecodeClientFrameRejectsUnmaskedClientData) {
  std::string buffer;
  buffer.push_back(static_cast<char>(0x81));
  buffer.push_back(static_cast<char>(0x02));
  buffer += "hi";

  std::string payload;
  uint8_t opcode = 0;
  bool isClose = false;

  EXPECT_FALSE(signaling::protocol::decodeClientFrame(buffer, payload, opcode, isClose));
}

TEST(ProtocolTest, DecodeClientFrameHandlesIncompleteBuffer) {
  std::string buffer;
  buffer.push_back(static_cast<char>(0x81));

  std::string payload;
  uint8_t opcode = 99;
  bool isClose = true;

  const bool ok = signaling::protocol::decodeClientFrame(buffer, payload, opcode, isClose);

  ASSERT_TRUE(ok);
  EXPECT_EQ(opcode, 0x00);
  EXPECT_FALSE(isClose);
  EXPECT_TRUE(payload.empty());
}

} // namespace

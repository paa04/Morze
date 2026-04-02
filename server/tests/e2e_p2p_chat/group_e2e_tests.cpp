#include "fixture/p2p_fixture.hpp"
#include "../e2e_signaling/clients/ws_test_client.hpp"

#include <gtest/gtest.h>

#include <boost/json/object.hpp>

namespace {

boost::json::object make_group_join_message(std::string_view room_id, std::string_view username)
{
    boost::json::object message;
    message["type"] = "join";
    message["roomId"] = room_id;
    message["username"] = username;
    message["roomType"] = "group";
    return message;
}

boost::json::object make_group_message(std::string_view room_id, const boost::json::value &payload)
{
    boost::json::object message;
    message["type"] = "group-message";
    message["roomId"] = room_id;
    message["payload"] = payload;
    return message;
}

} // namespace

TEST_F(TestFixtureP2p, group_message_is_broadcast_to_other_members)
{
    ws_test_client sender_client;
    ASSERT_TRUE(sender_client.connect("127.0.0.1", port));

    ws_test_client first_receiver_client;
    ASSERT_TRUE(first_receiver_client.connect("127.0.0.1", port));

    ws_test_client second_receiver_client;
    ASSERT_TRUE(second_receiver_client.connect("127.0.0.1", port));

    ASSERT_TRUE(sender_client.sendJson(make_group_join_message("group-room", "alice")));
    auto sender_join = sender_client.recvJson();
    ASSERT_TRUE(sender_join.is_object());
    const auto &sender_join_object = sender_join.as_object();
    ASSERT_TRUE(sender_join_object.contains("peerId"));
    const std::string sender_peer_id = std::string(sender_join_object.at("peerId").as_string().c_str());

    ASSERT_TRUE(first_receiver_client.sendJson(make_group_join_message("group-room", "bob")));
    auto first_receiver_join = first_receiver_client.recvJson();
    ASSERT_TRUE(first_receiver_join.is_object());
    auto sender_peer_joined = sender_client.recvJson();
    ASSERT_TRUE(sender_peer_joined.is_object());

    ASSERT_TRUE(second_receiver_client.sendJson(make_group_join_message("group-room", "carol")));
    auto second_receiver_join = second_receiver_client.recvJson();
    ASSERT_TRUE(second_receiver_join.is_object());
    auto sender_second_peer_joined = sender_client.recvJson();
    ASSERT_TRUE(sender_second_peer_joined.is_object());
    auto first_receiver_peer_joined = first_receiver_client.recvJson();
    ASSERT_TRUE(first_receiver_peer_joined.is_object());

    boost::json::object payload;
    payload["text"] = "hello group";
    payload["kind"] = "chat";

    ASSERT_TRUE(sender_client.sendJson(make_group_message("group-room", payload)));

    auto first_receiver_message = first_receiver_client.recvJson();
    ASSERT_TRUE(first_receiver_message.is_object());
    const auto &first_receiver_message_object = first_receiver_message.as_object();
    EXPECT_EQ(first_receiver_message_object.at("type").as_string(), "group-message");
    EXPECT_EQ(first_receiver_message_object.at("roomId").as_string(), "group-room");
    EXPECT_EQ(first_receiver_message_object.at("fromPeerId").as_string(), sender_peer_id);
    ASSERT_TRUE(first_receiver_message_object.at("payload").is_object());
    EXPECT_EQ(first_receiver_message_object.at("payload").as_object().at("text").as_string(), "hello group");

    auto second_receiver_message = second_receiver_client.recvJson();
    ASSERT_TRUE(second_receiver_message.is_object());
    const auto &second_receiver_message_object = second_receiver_message.as_object();
    EXPECT_EQ(second_receiver_message_object.at("type").as_string(), "group-message");
    EXPECT_EQ(second_receiver_message_object.at("roomId").as_string(), "group-room");
    EXPECT_EQ(second_receiver_message_object.at("fromPeerId").as_string(), sender_peer_id);
    ASSERT_TRUE(second_receiver_message_object.at("payload").is_object());
    EXPECT_EQ(second_receiver_message_object.at("payload").as_object().at("text").as_string(), "hello group");

    sender_client.close();
    first_receiver_client.close();
    second_receiver_client.close();
}

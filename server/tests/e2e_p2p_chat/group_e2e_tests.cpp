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

TEST_F(TestFixtureP2p, offline_member_receives_buffered_messages_on_reconnect)
{
    // 3 clients join group
    ws_test_client alice_client;
    ASSERT_TRUE(alice_client.connect("127.0.0.1", port));
    ws_test_client bob_client;
    ASSERT_TRUE(bob_client.connect("127.0.0.1", port));
    ws_test_client carol_client;
    ASSERT_TRUE(carol_client.connect("127.0.0.1", port));

    ASSERT_TRUE(alice_client.sendJson(make_group_join_message("buf-room", "alice")));
    alice_client.recvJson(); // joined

    ASSERT_TRUE(bob_client.sendJson(make_group_join_message("buf-room", "bob")));
    bob_client.recvJson(); // joined
    alice_client.recvJson(); // peer-joined(bob)

    ASSERT_TRUE(carol_client.sendJson(make_group_join_message("buf-room", "carol")));
    carol_client.recvJson(); // joined
    alice_client.recvJson(); // peer-joined(carol)
    bob_client.recvJson(); // peer-joined(carol)

    // Carol disconnects (offline but still member)
    carol_client.close();

    // Wait for server to process disconnect
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Consume peer-left notifications for carol
    alice_client.recvJson(); // peer-left(carol)
    bob_client.recvJson(); // peer-left(carol)

    // Alice sends 2 messages while carol is offline
    boost::json::object payload1;
    payload1["text"] = "message one";
    ASSERT_TRUE(alice_client.sendJson(make_group_message("buf-room", payload1)));

    boost::json::object payload2;
    payload2["text"] = "message two";
    ASSERT_TRUE(alice_client.sendJson(make_group_message("buf-room", payload2)));

    // Bob receives both live messages
    auto bob_msg1 = bob_client.recvJson();
    ASSERT_TRUE(bob_msg1.is_object());
    EXPECT_EQ(bob_msg1.as_object().at("type").as_string(), "group-message");
    EXPECT_TRUE(bob_msg1.as_object().contains("messageSeq"));

    auto bob_msg2 = bob_client.recvJson();
    ASSERT_TRUE(bob_msg2.is_object());
    EXPECT_EQ(bob_msg2.as_object().at("type").as_string(), "group-message");

    // Carol reconnects
    ws_test_client carol_reconnect;
    ASSERT_TRUE(carol_reconnect.connect("127.0.0.1", port));
    ASSERT_TRUE(carol_reconnect.sendJson(make_group_join_message("buf-room", "carol")));

    auto carol_joined = carol_reconnect.recvJson();
    ASSERT_TRUE(carol_joined.is_object());
    EXPECT_EQ(carol_joined.as_object().at("type").as_string(), "joined");

    // Carol should receive buffered-messages
    auto buffered = carol_reconnect.recvJson();
    ASSERT_TRUE(buffered.is_object());
    const auto &buffered_obj = buffered.as_object();
    EXPECT_EQ(buffered_obj.at("type").as_string(), "buffered-messages");
    EXPECT_EQ(buffered_obj.at("roomId").as_string(), "buf-room");

    const auto &messages = buffered_obj.at("messages").as_array();
    ASSERT_EQ(messages.size(), 2U);
    EXPECT_EQ(messages[0].as_object().at("payload").as_object().at("text").as_string(), "message one");
    EXPECT_EQ(messages[1].as_object().at("payload").as_object().at("text").as_string(), "message two");

    // Carol ACKs
    auto last_seq = messages[1].as_object().at("messageSeq").as_int64();
    boost::json::object ack;
    ack["type"] = "ack";
    ack["roomId"] = "buf-room";
    ack["upToSeq"] = last_seq;
    ASSERT_TRUE(carol_reconnect.sendJson(ack));

    auto ack_confirm = carol_reconnect.recvJson();
    ASSERT_TRUE(ack_confirm.is_object());
    EXPECT_EQ(ack_confirm.as_object().at("type").as_string(), "ack-confirm");
    EXPECT_EQ(ack_confirm.as_object().at("upToSeq").as_int64(), last_seq);

    alice_client.close();
    bob_client.close();
    carol_reconnect.close();
}

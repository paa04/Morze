#include "fixtures/signaling_e2e_fixture.hpp"
#include "clients/ws_test_client.hpp"

TEST_F(TestFixture, HappyPathJoin)
{
    ws_test_client TestClient1;
    ASSERT_TRUE(TestClient1.connect("127.0.0.1", port));

    ws_test_client TestClient2;
    ASSERT_TRUE(TestClient2.connect("127.0.0.1", port));

    boost::json::object obj1;
    obj1["type"] = "join";
    obj1["roomId"] = "r1";
    obj1["username"] = "alice";
    obj1["roomType"] = "direct";

    boost::json::object obj2;
    obj2["type"] = "join";
    obj2["roomId"] = "r1";
    obj2["username"] = "bob";
    obj2["roomType"] = "direct";

    ASSERT_TRUE(TestClient1.sendJson(obj1));

    auto first_ans = TestClient1.recvJson();
    ASSERT_TRUE(first_ans.is_object());
    const auto &first_ans_obj = first_ans.as_object();

    ASSERT_TRUE(first_ans_obj.contains("type"));
    EXPECT_EQ(first_ans_obj.at("type").as_string(), "joined");

    ASSERT_TRUE(first_ans_obj.contains("peerId"));
    EXPECT_TRUE(!first_ans_obj.at("peerId").as_string().empty());

    EXPECT_EQ(first_ans_obj.at("participants").as_array().size(), 0);

    ASSERT_TRUE(TestClient2.sendJson(obj2));
    auto second_ans = TestClient2.recvJson();
    ASSERT_TRUE(second_ans.is_object());
    const auto &second_obj = second_ans.as_object();

    ASSERT_TRUE(second_obj.contains("type"));
    EXPECT_EQ(second_obj.at("type").as_string(), "joined");

    ASSERT_TRUE(second_obj.contains("peerId"));
    EXPECT_TRUE(!second_obj.at("peerId").as_string().empty());

    EXPECT_EQ(second_obj.at("participants").as_array().size(), 1);

    auto third_ans = TestClient1.recvJson();
    ASSERT_TRUE(third_ans.is_object());
    const auto &third_obj = third_ans.as_object();

    ASSERT_TRUE(third_obj.contains("type"));
    EXPECT_EQ(third_obj.at("type").as_string(), "peer-joined");

    TestClient1.close();
    TestClient2.close();
}

TEST_F(TestFixture, LeaveTest)
{
    ws_test_client TestClient1;
    ASSERT_TRUE(TestClient1.connect("127.0.0.1", port));

    ws_test_client TestClient2;
    ASSERT_TRUE(TestClient2.connect("127.0.0.1", port));

    boost::json::object obj1;
    obj1["type"] = "join";
    obj1["roomId"] = "r1";
    obj1["username"] = "alice";
    obj1["roomType"] = "direct";

    boost::json::object obj2;
    obj2["type"] = "join";
    obj2["roomId"] = "r1";
    obj2["username"] = "bob";
    obj2["roomType"] = "direct";

    ASSERT_TRUE(TestClient1.sendJson(obj1));
    TestClient1.recvJson();
    ASSERT_TRUE(TestClient2.sendJson(obj2));

    TestClient2.recvJson();
    TestClient1.recvJson();


    TestClient1.close();

    auto ans = TestClient2.recvJson();

    ASSERT_TRUE(ans.is_object());

    const auto &obj = ans.as_object();
    ASSERT_TRUE(obj.contains("type"));
    EXPECT_EQ(obj.at("type").as_string(), "peer-left");
}

TEST_F(TestFixture, FullPathTest)
{
    ws_test_client TestClient1;
    ASSERT_TRUE(TestClient1.connect("127.0.0.1", port));

    ws_test_client TestClient2;
    ASSERT_TRUE(TestClient2.connect("127.0.0.1", port));

    boost::json::object obj1;
    obj1["type"] = "join";
    obj1["roomId"] = "r1";
    obj1["username"] = "alice";
    obj1["roomType"] = "direct";

    boost::json::object obj2;
    obj2["type"] = "join";
    obj2["roomId"] = "r1";
    obj2["username"] = "bob";
    obj2["roomType"] = "direct";

    ASSERT_TRUE(TestClient1.sendJson(obj1));
    auto jnObj = TestClient1.recvJson().as_object();
    auto peer_id1 = jnObj.at("peerId").as_string();
    ASSERT_TRUE(TestClient2.sendJson(obj2));

    auto jn2Obj = TestClient2.recvJson().as_object();
    auto peer_id2 = jn2Obj.at("peerId").as_string();

    auto peer_joined_id = TestClient1.recvJson().as_object().at("peerId").as_string();

    EXPECT_EQ(peer_id2, peer_joined_id);

    boost::json::object sdp;
    sdp["type"] = "offer";
    sdp["sdp"] = "v=0.0";

    boost::json::object offer;
    offer["type"] = "offer";
    offer["roomId"] = "r1";
    offer["toPeerId"] = peer_joined_id;
    offer["sdp"] = sdp;

    ASSERT_TRUE(TestClient1.sendJson(offer));

    auto offer_get = TestClient2.recvJson().as_object();
    EXPECT_EQ(offer_get.at("type").as_string(), "offer");
    EXPECT_EQ(offer_get.at("roomId").as_string(), "r1");
    EXPECT_EQ(offer_get.at("fromPeerId").as_string(), peer_id1);
    EXPECT_EQ(offer_get.at("toPeerId").as_string(), peer_id2);

    boost::json::object answer_sdp;
    answer_sdp["type"] = "answer";
    answer_sdp["sdp"] = "v=0.0";

    boost::json::object answer;
    answer["type"] = "answer";
    answer["roomId"] = "r1";
    answer["toPeerId"] = peer_id1;
    answer["sdp"] = answer_sdp;

    ASSERT_TRUE(TestClient2.sendJson(answer));

    auto ans_obj = TestClient1.recvJson().as_object();

    EXPECT_EQ(ans_obj.at("type").as_string(), "answer");
    EXPECT_EQ(ans_obj.at("roomId").as_string(), "r1");
    EXPECT_EQ(ans_obj.at("fromPeerId").as_string(), peer_id2);
    EXPECT_EQ(ans_obj.at("toPeerId").as_string(), peer_id1);
    EXPECT_EQ(ans_obj.at("sdp").as_object().at("type").as_string(), "answer");


}
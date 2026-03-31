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
    const auto& third_obj = third_ans.as_object();

    ASSERT_TRUE(third_obj.contains("type"));
    EXPECT_EQ(third_obj.at("type").as_string(), "peer-joined");

    TestClient1.close();
    TestClient2.close();
}
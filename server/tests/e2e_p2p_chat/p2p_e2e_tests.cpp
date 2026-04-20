//
// Created by paa04 on 01.04.2026.
//

#include "fixture/p2p_fixture.hpp"
#include "clients/p2p_chat_test_client.hpp"
#include "../e2e_signaling/clients/ws_test_client.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <rtc/candidate.hpp>
#include <rtc/description.hpp>

namespace {

struct local_signal
{
    enum class kind
    {
        description,
        candidate
    };

    kind type;
    std::string first;
    std::string second;
};

boost::json::object make_join_message(std::string_view room_id,
                                      std::string_view username,
                                      std::string_view room_type = "direct")
{
    boost::json::object message;
    message["type"] = "join";
    message["roomId"] = room_id;
    message["username"] = username;
    message["roomType"] = room_type;
    return message;
}

local_signal wait_for_local_signal(std::queue<local_signal> &signals,
                                   std::mutex &signals_mutex,
                                   std::condition_variable &signals_cv,
                                   std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(signals_mutex);
    const bool has_signal = signals_cv.wait_for(lock, timeout, [&signals]()
                                                {
                                                    return !signals.empty();
                                                });

    EXPECT_TRUE(has_signal);
    if (!has_signal)
        return {local_signal::kind::candidate, "", ""};

    auto signal = std::move(signals.front());
    signals.pop();
    return signal;
}

void push_local_description(std::queue<local_signal> &signals,
                            std::mutex &signals_mutex,
                            std::condition_variable &signals_cv,
                            std::string sdp,
                            std::string type)
{
    {
        std::lock_guard<std::mutex> lock(signals_mutex);
        signals.push(local_signal{local_signal::kind::description, std::move(sdp), std::move(type)});
    }
    signals_cv.notify_all();
}

void push_local_candidate(std::queue<local_signal> &signals,
                          std::mutex &signals_mutex,
                          std::condition_variable &signals_cv,
                          std::string candidate,
                          std::string mid)
{
    {
        std::lock_guard<std::mutex> lock(signals_mutex);
        signals.push(local_signal{local_signal::kind::candidate, std::move(candidate), std::move(mid)});
    }
    signals_cv.notify_all();
}

void forward_signal_through_server(ws_test_client &sender_signaling_client,
                                   ws_test_client &receiver_signaling_client,
                                   std::string_view room_id,
                                   std::string_view target_peer_id,
                                   const local_signal &signal,
                                   p2p_chat_test_client &receiver_peer)
{
    boost::json::object outbound_message;
    outbound_message["roomId"] = room_id;
    outbound_message["toPeerId"] = target_peer_id;

    if (signal.type == local_signal::kind::description)
    {
        rtc::Description forwarded_description(signal.first, signal.second);

        outbound_message["type"] = signal.second;
        outbound_message["sdp"] = boost::json::object{
                {"type", signal.second},
                {"sdp", signal.first}
        };

        ASSERT_TRUE(sender_signaling_client.sendJson(outbound_message));

        auto inbound_message = receiver_signaling_client.recvJson();
        ASSERT_TRUE(inbound_message.is_object());
        const auto &inbound_object = inbound_message.as_object();

        ASSERT_TRUE(inbound_object.contains("type"));
        ASSERT_TRUE(inbound_object.contains("sdp"));

        const auto &forwarded_sdp = inbound_object.at("sdp").as_object();
        receiver_peer.set_remote_description(
                rtc::Description(std::string(forwarded_sdp.at("sdp").as_string().c_str()),
                                 std::string(forwarded_sdp.at("type").as_string().c_str())));
        return;
    }

    outbound_message["type"] = "ice-candidate";
    outbound_message["candidate"] = boost::json::object{
            {"candidate", signal.first},
            {"sdpMid", signal.second}
    };

    ASSERT_TRUE(sender_signaling_client.sendJson(outbound_message));

    auto inbound_message = receiver_signaling_client.recvJson();
    ASSERT_TRUE(inbound_message.is_object());
    const auto &inbound_object = inbound_message.as_object();

    ASSERT_TRUE(inbound_object.contains("candidate"));
    const auto &forwarded_candidate = inbound_object.at("candidate").as_object();
    receiver_peer.add_remote_candidate(
            rtc::Candidate(std::string(forwarded_candidate.at("candidate").as_string().c_str()),
                           std::string(forwarded_candidate.at("sdpMid").as_string().c_str())));
}

} // namespace

TEST_F(TestFixtureP2p, happy_path_full)
{
    constexpr auto timeout = std::chrono::seconds(5);

    //Подключение к комнате
    ws_test_client first_signaling_client;
    ASSERT_TRUE(first_signaling_client.connect("127.0.0.1", port));

    ws_test_client second_signaling_client;
    ASSERT_TRUE(second_signaling_client.connect("127.0.0.1", port));

    ASSERT_TRUE(first_signaling_client.sendJson(make_join_message("p2p-room", "alice")));
    auto first_join = first_signaling_client.recvJson().as_object();
    const std::string first_peer_id = std::string(first_join.at("peerId").as_string().c_str());

    ASSERT_TRUE(second_signaling_client.sendJson(make_join_message("p2p-room", "bob")));
    auto second_join = second_signaling_client.recvJson().as_object();
    const std::string second_peer_id = std::string(second_join.at("peerId").as_string().c_str());

    auto peer_joined = first_signaling_client.recvJson().as_object();
    EXPECT_EQ(peer_joined.at("type").as_string(), "peer-joined");
    EXPECT_EQ(peer_joined.at("peerId").as_string(), second_peer_id);

    std::queue<local_signal> first_peer_signals;
    std::queue<local_signal> second_peer_signals;
    std::mutex first_peer_signals_mutex;
    std::mutex second_peer_signals_mutex;
    std::condition_variable first_peer_signals_cv;
    std::condition_variable second_peer_signals_cv;

    p2p_chat_test_client first_peer;
    p2p_chat_test_client second_peer;

    //Callbacks для webRTC

    first_peer.onLocalDescription([&](std::string sdp, std::string type)
                                  {
                                      push_local_description(first_peer_signals,
                                                             first_peer_signals_mutex,
                                                             first_peer_signals_cv,
                                                             std::move(sdp),
                                                             std::move(type));
                                  });

    second_peer.onLocalDescription([&](std::string sdp, std::string type)
                                   {
                                       push_local_description(second_peer_signals,
                                                              second_peer_signals_mutex,
                                                              second_peer_signals_cv,
                                                              std::move(sdp),
                                                              std::move(type));
                                   });

    first_peer.onLocalCandidate([&](std::string candidate, std::string mid)
                                {
                                    push_local_candidate(first_peer_signals,
                                                         first_peer_signals_mutex,
                                                         first_peer_signals_cv,
                                                         std::move(candidate),
                                                         std::move(mid));
                                });

    second_peer.onLocalCandidate([&](std::string candidate, std::string mid)
                                 {
                                     push_local_candidate(second_peer_signals,
                                                          second_peer_signals_mutex,
                                                          second_peer_signals_cv,
                                                          std::move(candidate),
                                                          std::move(mid));
                                 });

    first_peer.create_data_channel("chat");

    //Обмен sdp

    auto first_description = wait_for_local_signal(first_peer_signals,
                                                   first_peer_signals_mutex,
                                                   first_peer_signals_cv,
                                                   timeout);
    ASSERT_EQ(first_description.type, local_signal::kind::description);
    forward_signal_through_server(first_signaling_client,
                                  second_signaling_client,
                                  "p2p-room",
                                  second_peer_id,
                                  first_description,
                                  second_peer);

    auto second_description = wait_for_local_signal(second_peer_signals,
                                                    second_peer_signals_mutex,
                                                    second_peer_signals_cv,
                                                    timeout);
    ASSERT_EQ(second_description.type, local_signal::kind::description);
    forward_signal_through_server(second_signaling_client,
                                  first_signaling_client,
                                  "p2p-room",
                                  first_peer_id,
                                  second_description,
                                  first_peer);

    //Обмен ICE candidates

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while ((!first_peer.wait_until_open(std::chrono::milliseconds(10)) ||
            !second_peer.wait_until_open(std::chrono::milliseconds(10))) &&
           std::chrono::steady_clock::now() < deadline)
    {
        while (true)
        {
            std::unique_lock<std::mutex> lock(first_peer_signals_mutex);
            if (first_peer_signals.empty())
                break;
            auto signal = std::move(first_peer_signals.front());
            first_peer_signals.pop();
            lock.unlock();

            forward_signal_through_server(first_signaling_client,
                                          second_signaling_client,
                                          "p2p-room",
                                          second_peer_id,
                                          signal,
                                          second_peer);
        }

        while (true)
        {
            std::unique_lock<std::mutex> lock(second_peer_signals_mutex);
            if (second_peer_signals.empty())
                break;
            auto signal = std::move(second_peer_signals.front());
            second_peer_signals.pop();
            lock.unlock();

            forward_signal_through_server(second_signaling_client,
                                          first_signaling_client,
                                          "p2p-room",
                                          first_peer_id,
                                          signal,
                                          first_peer);
        }
    }

    ASSERT_TRUE(first_peer.wait_until_open(std::chrono::milliseconds(10)));
    ASSERT_TRUE(second_peer.wait_until_open(std::chrono::milliseconds(10)));

    //Отправка сообщений

    ASSERT_TRUE(first_peer.send_text("hello from first peer"));
    auto message_for_second_peer = second_peer.wait_for_message(timeout);
    ASSERT_TRUE(message_for_second_peer.has_value());
    EXPECT_EQ(*message_for_second_peer, "hello from first peer");

    ASSERT_TRUE(second_peer.send_text("hello from second peer"));
    auto message_for_first_peer = first_peer.wait_for_message(timeout);
    ASSERT_TRUE(message_for_first_peer.has_value());
    EXPECT_EQ(*message_for_first_peer, "hello from second peer");

    first_peer.close();
    second_peer.close();
    first_signaling_client.close();
    second_signaling_client.close();
}

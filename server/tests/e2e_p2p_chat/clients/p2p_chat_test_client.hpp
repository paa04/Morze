#ifndef MORZE_SIGNALING_SERVER_P2P_CHAT_TEST_CLIENT_HPP
#define MORZE_SIGNALING_SERVER_P2P_CHAT_TEST_CLIENT_HPP


#include <memory>
#include <rtc/peerconnection.hpp>
#include <condition_variable>
#include <queue>

class p2p_chat_test_client
{
    std::shared_ptr<rtc::PeerConnection> peer_connection;
    std::shared_ptr<rtc::DataChannel> data_channel;

    std::function<void(std::string sdp, std::string type)> local_description_cb;
    std::function<void(std::string candidate, std::string mid)> local_candidate_cb;

    void bind_data_channel(const std::shared_ptr<rtc::DataChannel> &channel);

    //Состояние
    bool data_channel_open = false;
    std::mutex mu;
    std::condition_variable cv;
    std::queue<std::string> messages;

public:

    p2p_chat_test_client()
    {
        rtc::Configuration config;
        config.iceServers.clear();

        peer_connection = std::make_shared<rtc::PeerConnection>(config);

        peer_connection->onLocalDescription([this](rtc::Description description)
                                            {
                                                if (local_description_cb)
                                                    local_description_cb(std::string(description),
                                                                         description.typeString());
                                            });

        peer_connection->onLocalCandidate([this](rtc::Candidate candidate)
                                          {
                                              if (local_candidate_cb)
                                                  local_candidate_cb(std::string(candidate), candidate.mid());
                                          }
        );

        peer_connection->onDataChannel([this](std::shared_ptr<rtc::DataChannel> channel)
                                       {
                                           bind_data_channel(channel);
                                       });
    }

    void onLocalDescription(std::function<void(std::string sdp, std::string type)> cb)
    {
        local_description_cb = std::move(cb);
    }

    void onLocalCandidate(std::function<void(std::string candidate, std::string mid)> cb)
    {
        local_candidate_cb = std::move(cb);
    }

    bool wait_until_open(std::chrono::milliseconds timeout);

    std::optional<std::string> wait_for_message(std::chrono::milliseconds timeout);


};

#endif

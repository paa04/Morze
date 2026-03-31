#include "clients/p2p_chat_test_client.hpp"

void p2p_chat_test_client::bind_data_channel(const std::shared_ptr<rtc::DataChannel> &channel)
{
    this->data_channel = channel;

    data_channel->onOpen([this]()
                         {
                             {
                                 std::lock_guard<std::mutex> lock(mu);
                                 data_channel_open = true;
                             }
                             cv.notify_all();
                         }
    );

    data_channel->onClosed([this]()
                           {
                               {
                                   std::lock_guard<std::mutex> lock(mu);
                                   data_channel_open = false;
                               }
                               cv.notify_all();
                           });

    data_channel->onMessage([this](rtc::message_variant data)
                            {
                                if (const auto* text = std::get_if<rtc::string>(&data))
                                {
                                    std::lock_guard<std::mutex> lock(mu);
                                    messages.push(*text);
                                }
                                cv.notify_all();
                            });


}

bool p2p_chat_test_client::wait_until_open(std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(mu);
    return cv.wait_for(lock, timeout, [this](){return data_channel_open;});
}

std::optional<std::string> p2p_chat_test_client::wait_for_message(std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(mu);
    bool res = cv.wait_for(lock, timeout, [this](){return !messages.empty();});

    if(!res) return std::nullopt;

    auto message = messages.front();
    messages.pop();
    return message;
}
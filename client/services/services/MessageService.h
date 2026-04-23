#ifndef MORZE_MESSAGESERVICE_H
#define MORZE_MESSAGESERVICE_H

#include <memory>
#include <vector>
#include <boost/asio/awaitable.hpp>
#include <boost/uuid/uuid.hpp>
#include "MessageModel.h"
#include "MessageRepository.h"

class MessageService {
public:
    explicit MessageService(std::shared_ptr<MessageRepository> repo);

    boost::asio::awaitable<std::vector<MessageModel>> getAllMessages();
    boost::asio::awaitable<MessageModel> getMessageById(boost::uuids::uuid messageId);
    boost::asio::awaitable<void> addMessage(const MessageModel& message);
    boost::asio::awaitable<void> updateMessage(const MessageModel& message);
    boost::asio::awaitable<void> removeMessage(boost::uuids::uuid messageId);

    boost::asio::awaitable<std::vector<MessageModel>> getMessagesByChatId(boost::uuids::uuid chatId);
    boost::asio::awaitable<std::vector<MessageModel>> getMessagesByChatAndSender(
        boost::uuids::uuid chatId, boost::uuids::uuid senderId);
    boost::asio::awaitable<std::vector<MessageModel>> getMessagesBySender(boost::uuids::uuid senderId);
    boost::asio::awaitable<std::vector<MessageModel>> getMessagesByChatFromDate(
        boost::uuids::uuid chatId, const std::chrono::system_clock::time_point& from);
    boost::asio::awaitable<std::vector<MessageModel>> getMessagesByChatSenderFromDate(
        boost::uuids::uuid chatId, boost::uuids::uuid senderId,
        const std::chrono::system_clock::time_point& from);

private:
    std::shared_ptr<MessageRepository> repo_;
};

#endif // MORZE_MESSAGESERVICE_H
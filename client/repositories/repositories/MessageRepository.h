//
// Created by ivan on 20.04.2026.
//

#ifndef MORZE_MESSAGEREPOSITORY_H
#define MORZE_MESSAGEREPOSITORY_H


#include <memory>
#include <vector>
#include <boost/asio/awaitable.hpp>
#include <boost/uuid/uuid.hpp>
#include "MessageModel.h"
#include "MessageDAO.h"
#include "DBConfiguration.h"

class MessageRepository {
public:
    using Storage = decltype(DBConfiguration::createStorage(""));

    explicit MessageRepository(boost::asio::io_context& ioc, std::shared_ptr<Storage> storage);

    // CRUD
    boost::asio::awaitable<std::vector<MessageModel>> getAllMessages() const;
    boost::asio::awaitable<MessageModel> getMessageById(boost::uuids::uuid messageId);
    boost::asio::awaitable<void> addMessage(const MessageDAO& message);
    boost::asio::awaitable<void> updateMessage(const MessageDAO& message);
    boost::asio::awaitable<void> removeMessage(boost::uuids::uuid messageId);

    // Специфичные выборки (все с сортировкой по убыванию created_at)
    // Все сообщения в чате
    boost::asio::awaitable<std::vector<MessageModel>> getMessagesByChatId(boost::uuids::uuid chatId);
    // Все сообщения в чате от указанного отправителя
    boost::asio::awaitable<std::vector<MessageModel>> getMessagesByChatAndSender(
        boost::uuids::uuid chatId, boost::uuids::uuid senderId);
    // Все сообщения от отправителя по всем чатам
    boost::asio::awaitable<std::vector<MessageModel>> getMessagesBySender(boost::uuids::uuid senderId);
    // Сообщения в чате начиная с указанной даты (включительно)
    boost::asio::awaitable<std::vector<MessageModel>> getMessagesByChatFromDate(
        boost::uuids::uuid chatId, const std::chrono::system_clock::time_point& from);
    // Сообщения в чате от отправителя начиная с даты
    boost::asio::awaitable<std::vector<MessageModel>> getMessagesByChatSenderFromDate(
        boost::uuids::uuid chatId, boost::uuids::uuid senderId,
        const std::chrono::system_clock::time_point& from);

private:
    boost::asio::io_context& ioc_;
    std::shared_ptr<Storage> storage_;
};


#endif //MORZE_MESSAGEREPOSITORY_H
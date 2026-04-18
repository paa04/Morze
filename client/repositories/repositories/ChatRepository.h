//
// Created by ivan on 18.04.2026.
//

#ifndef MORZE_CHATREPOSITORY_H
#define MORZE_CHATREPOSITORY_H

#include <memory>
#include <vector>
#include <boost/asio/awaitable.hpp>
#include <boost/uuid/uuid.hpp>

#include "ChatModel.h"
#include "DBConfiguration.h"
#include "ChatExcpetions.h"
#include "ChatDAOConverter.h"
#include "UUIDConverter.h"


class ChatRepository {
public:
    using Storage = decltype(DBConfiguration::createStorage(""));

    explicit ChatRepository(boost::asio::io_context& ioc, std::shared_ptr<Storage> storage);

    // Возвращает список всех чатов (моделей)
    boost::asio::awaitable<std::vector<ChatModel>> getAllChats() const;

    // Возвращает чат по ID, бросает NotFoundError, если не найден
    boost::asio::awaitable<ChatModel> getChatById(boost::uuids::uuid chatId);

    // Вставляет новый чат, бросает AlreadyExistsError, если такой chatId уже есть
    boost::asio::awaitable<void> AddChat(const ChatDAO& chat);

    // Обновляет существующий чат, бросает NotFoundError, если не найден
    boost::asio::awaitable<void> UpdateChat(const ChatDAO& chat);

    // Удаляет чат по ID
    boost::asio::awaitable<void> RemoveChat(boost::uuids::uuid chatId);

private:
    boost::asio::io_context& ioc_;
    std::shared_ptr<Storage> storage_;
};



#endif //MORZE_CHATREPOSITORY_H
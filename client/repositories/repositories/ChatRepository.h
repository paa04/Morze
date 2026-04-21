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

    // Чаты
    boost::asio::awaitable<std::vector<ChatModel>> getAllChats() const;
    boost::asio::awaitable<ChatModel> getChatById(boost::uuids::uuid chatId);
    boost::asio::awaitable<void> addChat(const ChatDAO& chat);
    boost::asio::awaitable<void> updateChat(const ChatDAO& chat);
    boost::asio::awaitable<void> removeChat(boost::uuids::uuid chatId);

    // Управление участниками чата (прямая работа с таблицей связей)
    boost::asio::awaitable<void> addMemberToChat(boost::uuids::uuid chatId, boost::uuids::uuid memberId);
    boost::asio::awaitable<void> removeMemberFromChat(boost::uuids::uuid chatId, boost::uuids::uuid memberId);
    boost::asio::awaitable<std::vector<ChatModel>> getChatsForMember(boost::uuids::uuid memberId);

private:
    boost::asio::io_context& ioc_;
    std::shared_ptr<Storage> storage_;
};

#endif //MORZE_CHATREPOSITORY_H
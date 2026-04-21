#ifndef MORZE_CHATSERVICE_H
#define MORZE_CHATSERVICE_H

#include <memory>
#include <vector>
#include <boost/asio/awaitable.hpp>
#include <boost/uuid/uuid.hpp>
#include "ChatModel.h"
#include "ChatRepository.h"

class ChatService {
public:
    explicit ChatService(std::shared_ptr<ChatRepository> repo);

    boost::asio::awaitable<std::vector<ChatModel>> getAllChats() const;
    boost::asio::awaitable<ChatModel> getChatById(boost::uuids::uuid chatId) const;
    boost::asio::awaitable<void> addChat(const ChatModel& chat) const;
    boost::asio::awaitable<void> updateChat(const ChatModel& chat) const;
    boost::asio::awaitable<void> removeChat(boost::uuids::uuid chatId) const;

    boost::asio::awaitable<void> addMemberToChat(boost::uuids::uuid chatId, boost::uuids::uuid memberId) const;
    boost::asio::awaitable<void> removeMemberFromChat(boost::uuids::uuid chatId, boost::uuids::uuid memberId) const;
    boost::asio::awaitable<std::vector<ChatModel>> getChatsForMember(boost::uuids::uuid memberId) const;

private:
    std::shared_ptr<ChatRepository> repo_;
};

#endif // MORZE_CHATSERVICE_H
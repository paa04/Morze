#ifndef MORZE_CHATMEMBERREPOSITORY_H
#define MORZE_CHATMEMBERREPOSITORY_H

#include <memory>
#include <vector>
#include <boost/asio/awaitable.hpp>
#include <boost/uuid/uuid.hpp>
#include "ChatMemberModel.h"
#include "ChatMemberDAO.h"
#include "DBConfiguration.h"

class ChatMemberRepository {
public:
    using Storage = decltype(DBConfiguration::createStorage(""));

    explicit ChatMemberRepository(boost::asio::io_context &ioc, std::shared_ptr<Storage> storage);

    // CRUD участников
    boost::asio::awaitable<std::vector<ChatMemberModel>> getAllMembers(bool orderByLastOnlineDesc = true) const;
    boost::asio::awaitable<ChatMemberModel> getMemberById(boost::uuids::uuid id);
    boost::asio::awaitable<void> addMember(const ChatMemberDAO &member);
    boost::asio::awaitable<void> updateMember(const ChatMemberDAO &member);
    boost::asio::awaitable<void> removeMember(boost::uuids::uuid id);

    // Участники конкретного чата
    boost::asio::awaitable<std::vector<ChatMemberModel>> getMembersByChatId(boost::uuids::uuid chatId);

private:
    boost::asio::io_context &ioc_;
    std::shared_ptr<Storage> storage_;
};

#endif //MORZE_CHATMEMBERREPOSITORY_H
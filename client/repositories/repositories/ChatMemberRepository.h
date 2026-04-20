//
// Created by ivan on 18.04.2026.
//

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

    // CRUD
    boost::asio::awaitable<std::vector<ChatMemberModel>> getAllMembers(bool orderByLastOnlineDesc = true) const;

    // Возвращает всех участников с заданным member_id (может быть несколько в разных чатах)
    boost::asio::awaitable<std::vector<ChatMemberModel>> getMembersByUserId(boost::uuids::uuid userId);

    boost::asio::awaitable<void> addMember(const ChatMemberDAO &member);

    boost::asio::awaitable<void> updateMember(const ChatMemberDAO &member);

    boost::asio::awaitable<void> removeMember(boost::uuids::uuid chatId, boost::uuids::uuid userId);

private:
    boost::asio::io_context &ioc_;
    std::shared_ptr<Storage> storage_;
};

#endif //MORZE_CHATMEMBERREPOSITORY_H
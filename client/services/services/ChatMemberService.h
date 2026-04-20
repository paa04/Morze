#ifndef MORZE_CHATMEMBERSERVICE_H
#define MORZE_CHATMEMBERSERVICE_H

#include <memory>
#include <vector>
#include <boost/asio/awaitable.hpp>
#include <boost/uuid/uuid.hpp>
#include "ChatMemberModel.h"
#include "ChatMemberRepository.h"

class ChatMemberService {
public:
    explicit ChatMemberService(std::shared_ptr<ChatMemberRepository> repo);

    boost::asio::awaitable<std::vector<ChatMemberModel>> getAllMembers(bool orderByLastOnlineDesc = true) const;
    boost::asio::awaitable<std::vector<ChatMemberModel>> getMembersByUserId(boost::uuids::uuid userId) const;
    boost::asio::awaitable<void> addMember(const ChatMemberModel& member) const;
    boost::asio::awaitable<void> updateMember(const ChatMemberModel& member) const;
    boost::asio::awaitable<void> removeMember(boost::uuids::uuid chatId, boost::uuids::uuid userId) const;

private:
    std::shared_ptr<ChatMemberRepository> repo_;
};

#endif // MORZE_CHATMEMBERSERVICE_H
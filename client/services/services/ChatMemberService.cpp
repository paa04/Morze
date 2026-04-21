#include "ChatMemberService.h"
#include "ChatMemberDAOConverter.h"
#include <iostream>
#include <boost/asio/use_awaitable.hpp>

ChatMemberService::ChatMemberService(std::shared_ptr<ChatMemberRepository> repo)
    : repo_(std::move(repo)) {}

boost::asio::awaitable<std::vector<ChatMemberModel>> ChatMemberService::getAllMembers(bool orderByLastOnlineDesc) const {
    try {
        co_return co_await repo_->getAllMembers(orderByLastOnlineDesc);
    } catch (const std::exception& e) {
        std::cerr << "[ChatMemberService] getAllMembers failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<ChatMemberModel> ChatMemberService::getMemberById(boost::uuids::uuid memberId) const {
    try {
        co_return co_await repo_->getMemberById(memberId);
    } catch (const std::exception& e) {
        std::cerr << "[ChatMemberService] getMemberById failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<std::vector<ChatMemberModel>> ChatMemberService::getMembersByChatId(boost::uuids::uuid chatId) const {
    try {
        co_return co_await repo_->getMembersByChatId(chatId);
    } catch (const std::exception& e) {
        std::cerr << "[ChatMemberService] getMembersByChatId failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<void> ChatMemberService::addMember(const ChatMemberModel& member) const {
    try {
        auto dao = ChatMemberDAOConverter::convert(member);
        co_await repo_->addMember(dao);
    } catch (const std::exception& e) {
        std::cerr << "[ChatMemberService] addMember failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<void> ChatMemberService::updateMember(const ChatMemberModel& member) const {
    try {
        auto dao = ChatMemberDAOConverter::convert(member);
        co_await repo_->updateMember(dao);
    } catch (const std::exception& e) {
        std::cerr << "[ChatMemberService] updateMember failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<void> ChatMemberService::removeMember(boost::uuids::uuid memberId) const {
    try {
        co_await repo_->removeMember(memberId);
    } catch (const std::exception& e) {
        std::cerr << "[ChatMemberService] removeMember failed: " << e.what() << '\n';
        throw;
    }
}
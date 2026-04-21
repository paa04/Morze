#include "ChatService.h"
#include "ChatDAOConverter.h"
#include <iostream>
#include <boost/asio/use_awaitable.hpp>

ChatService::ChatService(std::shared_ptr<ChatRepository> repo)
    : repo_(std::move(repo)) {}

boost::asio::awaitable<std::vector<ChatModel>> ChatService::getAllChats() const {
    try {
        co_return co_await repo_->getAllChats();
    } catch (const std::exception& e) {
        std::cerr << "[ChatService] getAllChats failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<ChatModel> ChatService::getChatById(const boost::uuids::uuid chatId) const {
    try {
        co_return co_await repo_->getChatById(chatId);
    } catch (const std::exception& e) {
        std::cerr << "[ChatService] getChatById failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<void> ChatService::addChat(const ChatModel& chat) const {
    try {
        auto dao = ChatDAOConverter::convert(chat);
        co_await repo_->addChat(dao);
    } catch (const std::exception& e) {
        std::cerr << "[ChatService] addChat failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<void> ChatService::updateChat(const ChatModel& chat) const {
    try {
        auto dao = ChatDAOConverter::convert(chat);
        co_await repo_->updateChat(dao);
    } catch (const std::exception& e) {
        std::cerr << "[ChatService] updateChat failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<void> ChatService::removeChat(const boost::uuids::uuid chatId) const {
    try {
        co_await repo_->removeChat(chatId);
    } catch (const std::exception& e) {
        std::cerr << "[ChatService] removeChat failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<void> ChatService::addMemberToChat(boost::uuids::uuid chatId, boost::uuids::uuid memberId) const {
    try {
        co_await repo_->addMemberToChat(chatId, memberId);
    } catch (const std::exception& e) {
        std::cerr << "[ChatService] addMemberToChat failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<void> ChatService::removeMemberFromChat(boost::uuids::uuid chatId, boost::uuids::uuid memberId) const {
    try {
        co_await repo_->removeMemberFromChat(chatId, memberId);
    } catch (const std::exception& e) {
        std::cerr << "[ChatService] removeMemberFromChat failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<std::vector<ChatModel>> ChatService::getChatsForMember(boost::uuids::uuid memberId) const {
    try {
        co_return co_await repo_->getChatsForMember(memberId);
    } catch (const std::exception& e) {
        std::cerr << "[ChatService] getChatsForMember failed: " << e.what() << '\n';
        throw;
    }
}
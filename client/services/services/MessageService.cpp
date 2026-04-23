#include "MessageService.h"
#include "MessageDAOConverter.h"
#include <iostream>
#include <boost/asio/use_awaitable.hpp>

MessageService::MessageService(std::shared_ptr<MessageRepository> repo)
    : repo_(std::move(repo)) {}

boost::asio::awaitable<std::vector<MessageModel>> MessageService::getAllMessages() {
    try {
        co_return co_await repo_->getAllMessages();
    } catch (const std::exception& e) {
        std::cerr << "[MessageService] getAllMessages failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<MessageModel> MessageService::getMessageById(boost::uuids::uuid messageId) {
    try {
        co_return co_await repo_->getMessageById(messageId);
    } catch (const std::exception& e) {
        std::cerr << "[MessageService] getMessageById failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<void> MessageService::addMessage(const MessageModel& message) {
    try {
        auto dao = MessageDAOConverter::convert(message);
        co_await repo_->addMessage(dao);
    } catch (const std::exception& e) {
        std::cerr << "[MessageService] addMessage failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<void> MessageService::updateMessage(const MessageModel& message) {
    try {
        auto dao = MessageDAOConverter::convert(message);
        co_await repo_->updateMessage(dao);
    } catch (const std::exception& e) {
        std::cerr << "[MessageService] updateMessage failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<void> MessageService::removeMessage(boost::uuids::uuid messageId) {
    try {
        co_await repo_->removeMessage(messageId);
    } catch (const std::exception& e) {
        std::cerr << "[MessageService] removeMessage failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<std::vector<MessageModel>> MessageService::getMessagesByChatId(boost::uuids::uuid chatId) {
    try {
        co_return co_await repo_->getMessagesByChatId(chatId);
    } catch (const std::exception& e) {
        std::cerr << "[MessageService] getMessagesByChatId failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<std::vector<MessageModel>> MessageService::getMessagesByChatAndSender(
    boost::uuids::uuid chatId, boost::uuids::uuid senderId)
{
    try {
        co_return co_await repo_->getMessagesByChatAndSender(chatId, senderId);
    } catch (const std::exception& e) {
        std::cerr << "[MessageService] getMessagesByChatAndSender failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<std::vector<MessageModel>> MessageService::getMessagesBySender(boost::uuids::uuid senderId) {
    try {
        co_return co_await repo_->getMessagesBySender(senderId);
    } catch (const std::exception& e) {
        std::cerr << "[MessageService] getMessagesBySender failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<std::vector<MessageModel>> MessageService::getMessagesByChatFromDate(
    boost::uuids::uuid chatId, const std::chrono::system_clock::time_point& from)
{
    try {
        co_return co_await repo_->getMessagesByChatFromDate(chatId, from);
    } catch (const std::exception& e) {
        std::cerr << "[MessageService] getMessagesByChatFromDate failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<std::vector<MessageModel>> MessageService::getMessagesByChatSenderFromDate(
    boost::uuids::uuid chatId, boost::uuids::uuid senderId,
    const std::chrono::system_clock::time_point& from)
{
    try {
        co_return co_await repo_->getMessagesByChatSenderFromDate(chatId, senderId, from);
    } catch (const std::exception& e) {
        std::cerr << "[MessageService] getMessagesByChatSenderFromDate failed: " << e.what() << '\n';
        throw;
    }
}
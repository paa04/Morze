#include "MessageRepository.h"
#include "MessageDAOConverter.h"
#include "UUIDConverter.h"
#include "TimePointConverter.h"
#include "MessageExceptions.h"
#include <boost/asio/post.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <sqlite_orm/sqlite_orm.h>

MessageRepository::MessageRepository(boost::asio::io_context& ioc, std::shared_ptr<Storage> storage)
    : ioc_(ioc), storage_(std::move(storage))
{}

boost::asio::awaitable<std::vector<MessageModel>> MessageRepository::getAllMessages() const {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto daoList = storage_->get_all<MessageDAO>(
        sqlite_orm::order_by(&MessageDAO::getCreatedAtAsUnixTime).desc()
    );
    std::vector<MessageModel> models;
    models.reserve(daoList.size());
    for (const auto& dao : daoList) {
        models.push_back(MessageDAOConverter::convert(dao));
    }
    co_return models;
}

boost::asio::awaitable<MessageModel> MessageRepository::getMessageById(boost::uuids::uuid messageId) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto blob = UUIDConverter::toBlob(messageId);
    auto results = storage_->get_all<MessageDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&MessageDAO::getIdAsBLOB, blob))
    );
    if (results.empty()) {
        throw MessageNotFoundError("Message not found");
    }
    co_return MessageDAOConverter::convert(results.front());
}

boost::asio::awaitable<void> MessageRepository::addMessage(const MessageDAO& message) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto blob = UUIDConverter::toBlob(message.getId());
    auto existing = storage_->get_all<MessageDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&MessageDAO::getIdAsBLOB, blob))
    );
    if (!existing.empty()) {
        throw MessageAlreadyExistsError("Message already exists");
    }
    storage_->replace(message);
    co_return;
}

boost::asio::awaitable<void> MessageRepository::updateMessage(const MessageDAO& message) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto blob = UUIDConverter::toBlob(message.getId());
    auto existing = storage_->get_all<MessageDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&MessageDAO::getIdAsBLOB, blob))
    );
    if (existing.empty()) {
        throw MessageNotFoundError("Message not found");
    }
    storage_->replace(message);
    co_return;
}

boost::asio::awaitable<void> MessageRepository::removeMessage(boost::uuids::uuid messageId) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto blob = UUIDConverter::toBlob(messageId);
    storage_->remove_all<MessageDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&MessageDAO::getIdAsBLOB, blob))
    );
    co_return;
}

boost::asio::awaitable<std::vector<MessageModel>> MessageRepository::getMessagesByChatId(boost::uuids::uuid chatId) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto blob = UUIDConverter::toBlob(chatId);
    auto daoList = storage_->get_all<MessageDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&MessageDAO::getChatIdAsBLOB, blob)),
        sqlite_orm::order_by(&MessageDAO::getCreatedAtAsUnixTime).desc()
    );
    std::vector<MessageModel> models;
    models.reserve(daoList.size());
    for (const auto& dao : daoList) {
        models.push_back(MessageDAOConverter::convert(dao));
    }
    co_return models;
}

boost::asio::awaitable<std::vector<MessageModel>> MessageRepository::getMessagesByChatAndSender(
    boost::uuids::uuid chatId, boost::uuids::uuid senderId)
{
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto chatBlob = UUIDConverter::toBlob(chatId);
    auto senderBlob = UUIDConverter::toBlob(senderId);
    auto daoList = storage_->get_all<MessageDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&MessageDAO::getChatIdAsBLOB, chatBlob) and
                          sqlite_orm::is_equal(&MessageDAO::getSenderIdAsBLOB, senderBlob)),
        sqlite_orm::order_by(&MessageDAO::getCreatedAtAsUnixTime).desc()
    );
    std::vector<MessageModel> models;
    models.reserve(daoList.size());
    for (const auto& dao : daoList) {
        models.push_back(MessageDAOConverter::convert(dao));
    }
    co_return models;
}

boost::asio::awaitable<std::vector<MessageModel>> MessageRepository::getMessagesBySender(boost::uuids::uuid senderId) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto senderBlob = UUIDConverter::toBlob(senderId);
    auto daoList = storage_->get_all<MessageDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&MessageDAO::getSenderIdAsBLOB, senderBlob)),
        sqlite_orm::order_by(&MessageDAO::getCreatedAtAsUnixTime).desc()
    );
    std::vector<MessageModel> models;
    models.reserve(daoList.size());
    for (const auto& dao : daoList) {
        models.push_back(MessageDAOConverter::convert(dao));
    }
    co_return models;
}

boost::asio::awaitable<std::vector<MessageModel>> MessageRepository::getMessagesByChatFromDate(
    boost::uuids::uuid chatId, const std::chrono::system_clock::time_point& from)
{
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto chatBlob = UUIDConverter::toBlob(chatId);
    auto unixFrom = TimePointConverter::toUnixSeconds(from);
    auto daoList = storage_->get_all<MessageDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&MessageDAO::getChatIdAsBLOB, chatBlob) and
                          sqlite_orm::greater_or_equal(&MessageDAO::getCreatedAtAsUnixTime, unixFrom)),
        sqlite_orm::order_by(&MessageDAO::getCreatedAtAsUnixTime).desc()
    );
    std::vector<MessageModel> models;
    models.reserve(daoList.size());
    for (const auto& dao : daoList) {
        models.push_back(MessageDAOConverter::convert(dao));
    }
    co_return models;
}

boost::asio::awaitable<std::vector<MessageModel>> MessageRepository::getMessagesByChatSenderFromDate(
    boost::uuids::uuid chatId, boost::uuids::uuid senderId,
    const std::chrono::system_clock::time_point& from)
{
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto chatBlob = UUIDConverter::toBlob(chatId);
    auto senderBlob = UUIDConverter::toBlob(senderId);
    auto unixFrom = TimePointConverter::toUnixSeconds(from);
    auto daoList = storage_->get_all<MessageDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&MessageDAO::getChatIdAsBLOB, chatBlob) and
                          sqlite_orm::is_equal(&MessageDAO::getSenderIdAsBLOB, senderBlob) and
                          sqlite_orm::greater_or_equal(&MessageDAO::getCreatedAtAsUnixTime, unixFrom)),
        sqlite_orm::order_by(&MessageDAO::getCreatedAtAsUnixTime).desc()
    );
    std::vector<MessageModel> models;
    models.reserve(daoList.size());
    for (const auto& dao : daoList) {
        models.push_back(MessageDAOConverter::convert(dao));
    }
    co_return models;
}
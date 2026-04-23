//
// Created by ivan on 18.04.2026.
//

#include <boost/asio/post.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/io_context.hpp>

#include "ChatRepository.h"
#include "ChatMemberExceptions.h"

ChatRepository::ChatRepository(boost::asio::io_context& ioc, std::shared_ptr<Storage> storage)
    : ioc_(ioc), storage_(std::move(storage))
{}

boost::asio::awaitable<std::vector<ChatModel>> ChatRepository::getAllChats() const {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    const auto daoList = storage_->get_all<ChatDAO>();
    std::vector<ChatModel> models;
    models.reserve(daoList.size());
    for (const auto& dao : daoList) {
        models.push_back(ChatDAOConverter::convert(dao));
    }
    co_return models;
}

boost::asio::awaitable<ChatModel> ChatRepository::getChatById(const boost::uuids::uuid chatId) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    const auto blob = UUIDConverter::toBlob(chatId);
    const auto results = storage_->get_all<ChatDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&ChatDAO::getIdAsBLOB, blob))
    );
    if (results.empty()) {
        throw ChatNotFoundError(std::string_view("Chat with id" +UUIDConverter::toString(chatId)+ "not found"));
    }
    co_return ChatDAOConverter::convert(results.front());
}

boost::asio::awaitable<void> ChatRepository::addChat(const ChatDAO &chat) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto blob = UUIDConverter::toBlob(chat.getId());
    auto existing = storage_->get_all<ChatDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&ChatDAO::getIdAsBLOB, blob))
    );
    if (!existing.empty()) {
        throw ChatAlreadyExistsError("Chat already exists");
    }
    storage_->replace(chat);
    co_return;
}

boost::asio::awaitable<void> ChatRepository::updateChat(const ChatDAO &chat) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto blob = UUIDConverter::toBlob(chat.getId());
    auto existing = storage_->get_all<ChatDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&ChatDAO::getIdAsBLOB, blob))
    );
    if (existing.empty()) {
        throw ChatNotFoundError("Chat not found");
    }
    storage_->replace(chat);
    co_return;
}


boost::asio::awaitable<void> ChatRepository::removeChat(boost::uuids::uuid chatId) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto blob = UUIDConverter::toBlob(chatId);
    storage_->remove_all<ChatDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&ChatDAO::getIdAsBLOB, blob))
    );
    co_return;
}

boost::asio::awaitable<void> ChatRepository::addMemberToChat(boost::uuids::uuid chatId, boost::uuids::uuid memberId) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);

    auto chatBlob = UUIDConverter::toBlob(chatId);
    auto memberBlob = UUIDConverter::toBlob(memberId);

    // Проверяем, существует ли уже такая связь
    auto existing = storage_->get_all<ChatMemberRelationDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&ChatMemberRelationDAO::getChatIdAsBLOB, chatBlob) and
                          sqlite_orm::is_equal(&ChatMemberRelationDAO::getMemberIdAsBLOB, memberBlob))
    );

    if (!existing.empty()) {
        throw ChatMemberAlreadyExistsError("Member already in this chat");
    }

    boost::uuids::random_generator gen;
    ChatMemberRelationDAO rel(gen(), chatId, memberId);
    storage_->replace(rel);
    co_return;
}

boost::asio::awaitable<void> ChatRepository::removeMemberFromChat(boost::uuids::uuid chatId, boost::uuids::uuid memberId) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto chatBlob = UUIDConverter::toBlob(chatId);
    auto memberBlob = UUIDConverter::toBlob(memberId);
    storage_->remove_all<ChatMemberRelationDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&ChatMemberRelationDAO::getChatIdAsBLOB, chatBlob) and
                          sqlite_orm::is_equal(&ChatMemberRelationDAO::getMemberIdAsBLOB, memberBlob))
    );
    co_return;
}

boost::asio::awaitable<std::vector<ChatModel>> ChatRepository::getChatsForMember(boost::uuids::uuid memberId) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto blob = UUIDConverter::toBlob(memberId);
    auto rels = storage_->get_all<ChatMemberRelationDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&ChatMemberRelationDAO::getMemberIdAsBLOB, blob))
    );
    std::vector<ChatModel> chats;
    for (const auto& rel : rels) {
        try {
            auto chat = co_await getChatById(rel.getChatId());
            chats.push_back(std::move(chat));
        } catch (const ChatNotFoundError&) {
            // чат мог быть удалён, пропускаем
        }
    }
    co_return chats;
}


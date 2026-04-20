//
// Created by ivan on 18.04.2026.
//

#include <boost/asio/post.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <sqlite_orm/sqlite_orm.h>

#include "ChatMemberRepository.h"
#include "ChatMemberDAOConverter.h"
#include "UUIDConverter.h"
#include "TimePointConverter.h"
#include "ChatMemberExceptions.h"

ChatMemberRepository::ChatMemberRepository(boost::asio::io_context& ioc, std::shared_ptr<Storage> storage)
    : ioc_(ioc), storage_(std::move(storage))
{}

boost::asio::awaitable<std::vector<ChatMemberModel>> ChatMemberRepository::getAllMembers(bool orderByLastOnlineDesc) const {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);

    std::vector<ChatMemberDAO> daoList;
    if (orderByLastOnlineDesc) {
        daoList = storage_->get_all<ChatMemberDAO>(
            sqlite_orm::order_by(&ChatMemberDAO::getLastOnlineAtAsUnix).desc()
        );
    } else {
        daoList = storage_->get_all<ChatMemberDAO>();
    }

    std::vector<ChatMemberModel> models;
    models.reserve(daoList.size());
    for (const auto& dao : daoList) {
        models.push_back(ChatMemberDAOConverter::convert(dao));
    }
    co_return models;
}

boost::asio::awaitable<std::vector<ChatMemberModel>> ChatMemberRepository::getMembersByUserId(boost::uuids::uuid userId) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto blob = UUIDConverter::toBlob(userId);
    auto results = storage_->get_all<ChatMemberDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&ChatMemberDAO::getUserIdAsBLOB, blob))
    );
    std::vector<ChatMemberModel> models;
    models.reserve(results.size());
    for (const auto& dao : results)
        models.push_back(ChatMemberDAOConverter::convert(dao));
    co_return models;
}

boost::asio::awaitable<void> ChatMemberRepository::addMember(const ChatMemberDAO& member) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto chatBlob = UUIDConverter::toBlob(member.getChatId());
    auto userBlob = UUIDConverter::toBlob(member.getUserId());
    auto existing = storage_->get_all<ChatMemberDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&ChatMemberDAO::getChatIdAsBLOB, chatBlob) and
                          sqlite_orm::is_equal(&ChatMemberDAO::getUserIdAsBLOB, userBlob))
    );
    if (!existing.empty())
        throw ChatMemberAlreadyExistsError("User already in this chat");
    storage_->replace(member);
    co_return;
}

boost::asio::awaitable<void> ChatMemberRepository::updateMember(const ChatMemberDAO& member) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    storage_->update(member);  // составной ключ позволяет использовать update
    co_return;
}

boost::asio::awaitable<void> ChatMemberRepository::removeMember(boost::uuids::uuid chatId, boost::uuids::uuid userId) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto chatBlob = UUIDConverter::toBlob(chatId);
    auto userBlob = UUIDConverter::toBlob(userId);
    storage_->remove_all<ChatMemberDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&ChatMemberDAO::getChatIdAsBLOB, chatBlob) and
                          sqlite_orm::is_equal(&ChatMemberDAO::getUserIdAsBLOB, userBlob))
    );
    co_return;
}
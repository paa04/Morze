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

boost::asio::awaitable<ChatMemberModel> ChatMemberRepository::getMemberById(boost::uuids::uuid id) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    const auto blob = UUIDConverter::toBlob(id);
    const auto results = storage_->get_all<ChatMemberDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&ChatMemberDAO::getIdAsBLOB, blob))
    );
    if (results.empty()) {
        throw ChatMemberNotFoundError("Chat member not found");
    }
    co_return ChatMemberDAOConverter::convert(results.front());
}

boost::asio::awaitable<void> ChatMemberRepository::addMember(const ChatMemberDAO& member) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto blob = UUIDConverter::toBlob(member.getId());
    auto existing = storage_->get_all<ChatMemberDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&ChatMemberDAO::getIdAsBLOB, blob))
    );
    if (!existing.empty()) {
        throw ChatMemberAlreadyExistsError("Chat member already exists");
    }
    storage_->replace(member);
    co_return;
}

boost::asio::awaitable<void> ChatMemberRepository::updateMember(const ChatMemberDAO& member) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto blob = UUIDConverter::toBlob(member.getId());
    auto existing = storage_->get_all<ChatMemberDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&ChatMemberDAO::getIdAsBLOB, blob))
    );
    if (existing.empty()) {
        throw ChatMemberNotFoundError("Chat member not found");
    }
    storage_->replace(member);
    co_return;
}

boost::asio::awaitable<void> ChatMemberRepository::removeMember(boost::uuids::uuid id) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto blob = UUIDConverter::toBlob(id);
    storage_->remove_all<ChatMemberDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&ChatMemberDAO::getIdAsBLOB, blob))
    );
    co_return;
}

boost::asio::awaitable<std::vector<ChatMemberModel>> ChatMemberRepository::getMembersByUsername(const std::string& username, bool orderByLastOnlineDesc) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto condition = sqlite_orm::where(sqlite_orm::is_equal(&ChatMemberDAO::getUsername, username));
    std::vector<ChatMemberDAO> daoList;
    if (orderByLastOnlineDesc) {
        daoList = storage_->get_all<ChatMemberDAO>(condition,
            sqlite_orm::order_by(&ChatMemberDAO::getLastOnlineAtAsUnix).desc()
        );
    } else {
        daoList = storage_->get_all<ChatMemberDAO>(condition);
    }
    std::vector<ChatMemberModel> models;
    models.reserve(daoList.size());
    for (const auto& dao : daoList) {
        models.push_back(ChatMemberDAOConverter::convert(dao));
    }
    co_return models;
}
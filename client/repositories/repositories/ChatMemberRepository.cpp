#include "ChatMemberRepository.h"
#include "ChatMemberDAOConverter.h"
#include "ChatMemberRelationDAO.h"
#include "UUIDConverter.h"
#include "ChatMemberExceptions.h"
#include <boost/asio/post.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <sqlite_orm/sqlite_orm.h>
#include <boost/asio/io_context.hpp>

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
    auto blob = UUIDConverter::toBlob(id);
    auto results = storage_->get_all<ChatMemberDAO>(
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

boost::asio::awaitable<std::vector<ChatMemberModel>> ChatMemberRepository::getMembersByChatId(boost::uuids::uuid chatId) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto blob = UUIDConverter::toBlob(chatId);
    auto rels = storage_->get_all<ChatMemberRelationDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&ChatMemberRelationDAO::getChatIdAsBLOB, blob))
    );
    std::vector<ChatMemberModel> members;
    for (const auto& rel : rels) {
        try {
            auto member = co_await getMemberById(rel.getMemberId());
            members.push_back(std::move(member));
        } catch (const ChatMemberNotFoundError&) {
            // Участник мог быть удалён, пропускаем
        }
    }
    co_return members;
}
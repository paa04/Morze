#include "UserRepository.h"

#include <boost/asio/io_context.hpp>

#include "UserDAOConverter.h"
#include "UUIDConverter.h"
#include "TimePointConverter.h"
#include "UserExceptions.h"
#include <boost/asio/post.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <sqlite_orm/sqlite_orm.h>

UserRepository::UserRepository(boost::asio::io_context& ioc, std::shared_ptr<Storage> storage)
    : ioc_(ioc), storage_(std::move(storage)) {}

boost::asio::awaitable<std::vector<UserModel>> UserRepository::getAllUsers() const {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto daoList = storage_->get_all<UserDAO>();
    std::vector<UserModel> models;
    models.reserve(daoList.size());
    for (const auto& dao : daoList) {
        models.push_back(UserDAOConverter::convert(dao));
    }
    co_return models;
}

boost::asio::awaitable<UserModel> UserRepository::getUserByLogin(const std::string& login) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto results = storage_->get_all<UserDAO>(
        sqlite_orm::where(sqlite_orm::c(&UserDAO::getLogin) == login)
    );
    if (results.empty()) {
        throw UserNotFoundError("User not found");
    }
    co_return UserDAOConverter::convert(results.front());
}

boost::asio::awaitable<UserModel> UserRepository::getUserByMemberId(boost::uuids::uuid memberId) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto blob = UUIDConverter::toBlob(memberId);
    auto results = storage_->get_all<UserDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&UserDAO::getMemberIdAsBLOB, blob))
    );
    if (results.empty()) {
        throw UserNotFoundError("User not found");
    }
    co_return UserDAOConverter::convert(results.front());
}

boost::asio::awaitable<void> UserRepository::addUser(const UserDAO& user) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto existing = storage_->get_all<UserDAO>(
        sqlite_orm::where(sqlite_orm::c(&UserDAO::getLogin) == user.getLogin())
    );
    if (!existing.empty()) {
        throw UserAlreadyExistsError("User already exists");
    }
    storage_->replace(user);
    co_return;
}

boost::asio::awaitable<void> UserRepository::updateUser(const UserDAO& user) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto existing = storage_->get_all<UserDAO>(
        sqlite_orm::where(sqlite_orm::c(&UserDAO::getLogin) == user.getLogin())
    );
    if (existing.empty()) {
        throw UserNotFoundError("User not found");
    }
    storage_->replace(user);
    co_return;
}

boost::asio::awaitable<void> UserRepository::removeUser(const std::string& login) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    storage_->remove_all<UserDAO>(
        sqlite_orm::where(sqlite_orm::c(&UserDAO::getLogin) == login)
    );
    co_return;
}
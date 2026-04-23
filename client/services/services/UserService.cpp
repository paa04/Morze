#include "UserService.h"
#include "UserDAOConverter.h"
#include <iostream>
#include <boost/asio/use_awaitable.hpp>

UserService::UserService(std::shared_ptr<UserRepository> repo)
    : repo_(std::move(repo)) {}

boost::asio::awaitable<std::vector<UserModel>> UserService::getAllUsers() const {
    try {
        co_return co_await repo_->getAllUsers();
    } catch (const std::exception& e) {
        std::cerr << "[UserService] getAllUsers failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<UserModel> UserService::getUserByLogin(const std::string& login) const {
    try {
        co_return co_await repo_->getUserByLogin(login);
    } catch (const std::exception& e) {
        std::cerr << "[UserService] getUserByLogin failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<UserModel> UserService::getUserByMemberId(boost::uuids::uuid memberId) const {
    try {
        co_return co_await repo_->getUserByMemberId(memberId);
    } catch (const std::exception& e) {
        std::cerr << "[UserService] getUserByMemberId failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<void> UserService::addUser(const UserModel& user) const {
    try {
        auto dao = UserDAOConverter::convert(user);
        co_await repo_->addUser(dao);
    } catch (const std::exception& e) {
        std::cerr << "[UserService] addUser failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<void> UserService::updateUser(const UserModel& user) const {
    try {
        auto dao = UserDAOConverter::convert(user);
        co_await repo_->updateUser(dao);
    } catch (const std::exception& e) {
        std::cerr << "[UserService] updateUser failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<void> UserService::removeUser(const std::string& login) const {
    try {
        co_await repo_->removeUser(login);
    } catch (const std::exception& e) {
        std::cerr << "[UserService] removeUser failed: " << e.what() << '\n';
        throw;
    }
}
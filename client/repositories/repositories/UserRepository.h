#ifndef MORZE_USERREPOSITORY_H
#define MORZE_USERREPOSITORY_H

#include <memory>
#include <vector>
#include <boost/asio/awaitable.hpp>
#include "UserModel.h"
#include "UserDAO.h"
#include "DBConfiguration.h"

class UserRepository {
public:
    using Storage = decltype(DBConfiguration::createStorage(""));

    explicit UserRepository(boost::asio::io_context& ioc, std::shared_ptr<Storage> storage);

    boost::asio::awaitable<std::vector<UserModel>> getAllUsers() const;
    boost::asio::awaitable<UserModel> getUserByLogin(const std::string& login);
    boost::asio::awaitable<UserModel> getUserByMemberId(boost::uuids::uuid memberId);
    boost::asio::awaitable<void> addUser(const UserDAO& user);
    boost::asio::awaitable<void> updateUser(const UserDAO& user);
    boost::asio::awaitable<void> removeUser(const std::string& login);

private:
    boost::asio::io_context& ioc_;
    std::shared_ptr<Storage> storage_;
};

#endif //MORZE_USERREPOSITORY_H
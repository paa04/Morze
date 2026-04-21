#ifndef MORZE_USERSERVICE_H
#define MORZE_USERSERVICE_H

#include <memory>
#include <vector>
#include <boost/asio/awaitable.hpp>
#include <boost/uuid/uuid.hpp>
#include "UserModel.h"
#include "UserRepository.h"

class UserService {
public:
    explicit UserService(std::shared_ptr<UserRepository> repo);

    boost::asio::awaitable<std::vector<UserModel>> getAllUsers() const;
    boost::asio::awaitable<UserModel> getUserByLogin(const std::string& login) const;
    boost::asio::awaitable<UserModel> getUserByMemberId(boost::uuids::uuid memberId) const;
    boost::asio::awaitable<void> addUser(const UserModel& user) const;
    boost::asio::awaitable<void> updateUser(const UserModel& user) const;
    boost::asio::awaitable<void> removeUser(const std::string& login) const;

private:
    std::shared_ptr<UserRepository> repo_;
};

#endif // MORZE_USERSERVICE_H
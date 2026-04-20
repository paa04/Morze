//
// Created by ivan on 20.04.2026.
//
#ifndef MORZE_CONNECTIONPROFILEREPOSITORY_H
#define MORZE_CONNECTIONPROFILEREPOSITORY_H

#include <memory>
#include <vector>
#include <boost/asio/awaitable.hpp>
#include <boost/uuid/uuid.hpp>
#include "ConnectionProfileModel.h"
#include "ConnectionProfileDAO.h"
#include "DBConfiguration.h"

class ConnectionProfileRepository {
public:
    using Storage = decltype(DBConfiguration::createStorage(""));

    explicit ConnectionProfileRepository(boost::asio::io_context &ioc, std::shared_ptr<Storage> storage);

    // CRUD
    boost::asio::awaitable<std::vector<ConnectionProfileModel> > getAllProfiles(bool orderByUpdatedAtDesc = true) const;

    boost::asio::awaitable<ConnectionProfileModel> getProfileById(boost::uuids::uuid id);

    boost::asio::awaitable<void> addProfile(const ConnectionProfileDAO &profile);

    boost::asio::awaitable<void> updateProfile(const ConnectionProfileDAO &profile);

    boost::asio::awaitable<void> removeProfile(boost::uuids::uuid id);

    // Специфичные методы
    boost::asio::awaitable<ConnectionProfileModel> getLatestProfile();

private:
    boost::asio::io_context &ioc_;
    std::shared_ptr<Storage> storage_;
};

#endif //MORZE_CONNECTIONPROFILEREPOSITORY_H

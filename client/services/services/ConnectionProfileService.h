#ifndef MORZE_CONNECTIONPROFILESERVICE_H
#define MORZE_CONNECTIONPROFILESERVICE_H

#include <memory>
#include <vector>
#include <boost/asio/awaitable.hpp>
#include <boost/uuid/uuid.hpp>
#include "ConnectionProfileModel.h"
#include "ConnectionProfileRepository.h"

class ConnectionProfileService {
public:
    explicit ConnectionProfileService(std::shared_ptr<ConnectionProfileRepository> repo);

    boost::asio::awaitable<std::vector<ConnectionProfileModel>> getAllProfiles(bool orderByUpdatedAtDesc = true);
    boost::asio::awaitable<ConnectionProfileModel> getProfileById(boost::uuids::uuid id);
    boost::asio::awaitable<void> addProfile(const ConnectionProfileModel& profile);
    boost::asio::awaitable<void> updateProfile(const ConnectionProfileModel& profile);
    boost::asio::awaitable<void> removeProfile(boost::uuids::uuid id);
    boost::asio::awaitable<ConnectionProfileModel> getLatestProfile();

private:
    std::shared_ptr<ConnectionProfileRepository> repo_;
};

#endif // MORZE_CONNECTIONPROFILESERVICE_H
#include "ConnectionProfileService.h"
#include "ConnectionProfileDAOConverter.h"
#include <iostream>
#include <boost/asio/use_awaitable.hpp>

ConnectionProfileService::ConnectionProfileService(std::shared_ptr<ConnectionProfileRepository> repo)
    : repo_(std::move(repo)) {}

boost::asio::awaitable<std::vector<ConnectionProfileModel>> ConnectionProfileService::getAllProfiles(bool orderByUpdatedAtDesc) {
    try {
        co_return co_await repo_->getAllProfiles(orderByUpdatedAtDesc);
    } catch (const std::exception& e) {
        std::cerr << "[ConnectionProfileService] getAllProfiles failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<ConnectionProfileModel> ConnectionProfileService::getProfileById(boost::uuids::uuid id) {
    try {
        co_return co_await repo_->getProfileById(id);
    } catch (const std::exception& e) {
        std::cerr << "[ConnectionProfileService] getProfileById failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<void> ConnectionProfileService::addProfile(const ConnectionProfileModel& profile) {
    try {
        auto dao = ConnectionProfileDAOConverter::convert(profile);
        co_await repo_->addProfile(dao);
    } catch (const std::exception& e) {
        std::cerr << "[ConnectionProfileService] addProfile failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<void> ConnectionProfileService::updateProfile(const ConnectionProfileModel& profile) {
    try {
        auto dao = ConnectionProfileDAOConverter::convert(profile);
        co_await repo_->updateProfile(dao);
    } catch (const std::exception& e) {
        std::cerr << "[ConnectionProfileService] updateProfile failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<void> ConnectionProfileService::removeProfile(boost::uuids::uuid id) {
    try {
        co_await repo_->removeProfile(id);
    } catch (const std::exception& e) {
        std::cerr << "[ConnectionProfileService] removeProfile failed: " << e.what() << '\n';
        throw;
    }
}

boost::asio::awaitable<ConnectionProfileModel> ConnectionProfileService::getLatestProfile() {
    try {
        co_return co_await repo_->getLatestProfile();
    } catch (const std::exception& e) {
        std::cerr << "[ConnectionProfileService] getLatestProfile failed: " << e.what() << '\n';
        throw;
    }
}
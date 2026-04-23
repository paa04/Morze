//
// Created by ivan on 20.04.2026.
//

#include "ConnectionProfileRepository.h"
#include "ConnectionProfileDAOConverter.h"
#include "UUIDConverter.h"
#include "ConnectionProfileExceptions.h"
#include <boost/asio/post.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <sqlite_orm/sqlite_orm.h>

ConnectionProfileRepository::ConnectionProfileRepository(boost::asio::io_context &ioc, std::shared_ptr<Storage> storage)
    : ioc_(ioc), storage_(std::move(storage)) {
}

boost::asio::awaitable<std::vector<ConnectionProfileModel> > ConnectionProfileRepository::getAllProfiles(
    bool orderByUpdatedAtDesc) const {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);

    std::vector<ConnectionProfileDAO> daoList;
    if (orderByUpdatedAtDesc) {
        daoList = storage_->get_all<ConnectionProfileDAO>(
            sqlite_orm::order_by(&ConnectionProfileDAO::getUpdatedAtAsUnix).desc()
        );
    } else {
        daoList = storage_->get_all<ConnectionProfileDAO>();
    }

    std::vector<ConnectionProfileModel> models;
    models.reserve(daoList.size());
    for (const auto &dao: daoList) {
        models.push_back(ConnectionProfileDAOConverter::convert(dao));
    }
    co_return models;
}

boost::asio::awaitable<ConnectionProfileModel> ConnectionProfileRepository::getProfileById(boost::uuids::uuid id) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    const auto blob = UUIDConverter::toBlob(id);
    const auto results = storage_->get_all<ConnectionProfileDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&ConnectionProfileDAO::getIdAsBLOB, blob))
    );
    if (results.empty()) {
        throw ConnectionProfileNotFoundError("Connection profile not found");
    }
    co_return ConnectionProfileDAOConverter::convert(results.front());
}

boost::asio::awaitable<void> ConnectionProfileRepository::addProfile(const ConnectionProfileDAO &profile) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto blob = UUIDConverter::toBlob(profile.getId());
    auto existing = storage_->get_all<ConnectionProfileDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&ConnectionProfileDAO::getIdAsBLOB, blob))
    );
    if (!existing.empty()) {
        throw ConnectionProfileAlreadyExistsError("Connection profile already exists");
    }
    storage_->replace(profile);
    co_return;
}

boost::asio::awaitable<void> ConnectionProfileRepository::updateProfile(const ConnectionProfileDAO &profile) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto blob = UUIDConverter::toBlob(profile.getId());
    auto existing = storage_->get_all<ConnectionProfileDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&ConnectionProfileDAO::getIdAsBLOB, blob))
    );
    if (existing.empty()) {
        throw ConnectionProfileNotFoundError("Connection profile not found");
    }
    storage_->replace(profile);
    co_return;
}

boost::asio::awaitable<void> ConnectionProfileRepository::removeProfile(boost::uuids::uuid id) {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto blob = UUIDConverter::toBlob(id);
    storage_->remove_all<ConnectionProfileDAO>(
        sqlite_orm::where(sqlite_orm::is_equal(&ConnectionProfileDAO::getIdAsBLOB, blob))
    );
    co_return;
}

boost::asio::awaitable<ConnectionProfileModel> ConnectionProfileRepository::getLatestProfile() {
    co_await boost::asio::post(ioc_.get_executor(), boost::asio::use_awaitable);
    auto daoList = storage_->get_all<ConnectionProfileDAO>(
        sqlite_orm::order_by(&ConnectionProfileDAO::getUpdatedAtAsUnix).desc(),
        sqlite_orm::limit(1)
    );
    if (daoList.empty()) {
        throw ConnectionProfileNotFoundError("No connection profiles found");
    }
    co_return ConnectionProfileDAOConverter::convert(daoList.front());
}

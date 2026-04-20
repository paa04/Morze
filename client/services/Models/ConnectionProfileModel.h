#ifndef MORZE_CONNECTIONPROFILEMODEL_H
#define MORZE_CONNECTIONPROFILEMODEL_H

#include <stdexcept>
#include <string>
#include <chrono>
#include <boost/uuid/uuid.hpp>

#include "UUIDConverter.h"
#include "TimePointConverter.h"

class ConnectionProfileModel {
public:
    ConnectionProfileModel() = default;

    ConnectionProfileModel(const boost::uuids::uuid id,
                         std::string server_url,
                         std::string stun_url,
                         const std::chrono::system_clock::time_point updated_at)
        : id_(id)
        , server_url_(std::move(server_url))
        , stun_url_(std::move(stun_url))
        , updated_at_(updated_at) {
        validateInvariants();
    }

    ConnectionProfileModel(std::string server_url,
                         std::string stun_url,
                         const std::chrono::system_clock::time_point updated_at)
    : server_url_(std::move(server_url))
    , stun_url_(std::move(stun_url))
    , updated_at_(updated_at)
    {
        validateInvariants();
        boost::uuids::random_generator gen;
        id_ = gen();
    }

    // Геттеры
    const boost::uuids::uuid& getId() const { return id_; }
    std::vector<char> getIdAsBLOB() const { return UUIDConverter::toBlob(id_); }
    std::string getIdAsString() const { return UUIDConverter::toString(id_); }

    const std::string& getServerUrl() const { return server_url_; }
    const std::string& getStunUrl() const { return stun_url_; }

    const std::chrono::system_clock::time_point& getUpdatedAt() const { return updated_at_; }
    std::int64_t getUpdatedAtAsUnix() const { return TimePointConverter::toUnixSeconds(updated_at_); }
    std::string getUpdatedAtAsString() const { return TimePointConverter::toIsoString(updated_at_); }

    // Сеттеры
    void setId(const boost::uuids::uuid id) { id_ = id; }
    void setIdFromBLOB(const std::vector<char>& blob) { id_ = UUIDConverter::fromBlob(blob); }
    void setIdFromString(const std::string& str) { id_ = UUIDConverter::fromString(str); }

    void setServerUrl(std::string url) {
        if (url.empty()) throw std::invalid_argument("server_url cannot be empty");
        server_url_ = std::move(url);
    }

    void setStunUrl(std::string url) {
        stun_url_ = std::move(url);
    }

    void setUpdatedAt(const std::chrono::system_clock::time_point updated_at) {
        updated_at_ = updated_at;
    }
    void setUpdatedAtFromUnix(const std::int64_t sec) {
        updated_at_ = TimePointConverter::fromUnixSeconds(sec);
    }
    void setUpdatedAtFromString(const std::string& iso) {
        updated_at_ = TimePointConverter::fromIsoString(iso);
    }

private:
    boost::uuids::uuid id_;
    std::string server_url_;
    std::string stun_url_;
    std::chrono::system_clock::time_point updated_at_;

    void validateInvariants() const {
        if (server_url_.empty()) throw std::invalid_argument("server_url is required");
        // stun_url может быть пустым
    }
};

#endif //MORZE_CONNECTIONPROFILEMODEL_H
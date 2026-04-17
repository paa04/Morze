//
// Created by ivan on 17.04.2026.
//

#ifndef MORZE_CONNECTIONPROFILEDAO_H
#define MORZE_CONNECTIONPROFILEDAO_H
#include <stdexcept>
#include <string>
#include <boost/uuid/uuid.hpp>


class ConnectionProfileDAO {
public:
    ConnectionProfileDAO() = default;

    ConnectionProfileDAO(boost::uuids::uuid id,
                         std::string server_url,
                         std::string stun_url,
                         std::chrono::system_clock::time_point updated_at)
        : id_(std::move(id))
          , server_url_(std::move(server_url))
          , stun_url_(std::move(stun_url))
          , updated_at_(updated_at) {
        validateInvariants();
    }

    const boost::uuids::uuid &getId() const { return id_; }
    const std::string &getServerUrl() const { return server_url_; }
    const std::string &getStunUrl() const { return stun_url_; }
    const std::chrono::system_clock::time_point &getUpdatedAt() const { return updated_at_; }

    void setId(boost::uuids::uuid id) {
        id_ = std::move(id);
    }

    void setServerUrl(std::string url) {
        if (url.empty()) throw std::invalid_argument("server_url cannot be empty");
        server_url_ = std::move(url);
    }

    void setStunUrl(std::string url) {
        if (stun_url_.empty()) throw std::invalid_argument("stun_url is required");
        stun_url_ = std::move(url);
    }

    void setUpdatedAt(std::chrono::system_clock::time_point updated_at) {
        updated_at_ = updated_at;
    }

private:
    boost::uuids::uuid id_;
    std::string server_url_;
    std::string stun_url_;
    std::chrono::system_clock::time_point updated_at_;

    void validateInvariants() const {
        if (server_url_.empty()) throw std::invalid_argument("server_url is required");
        if (stun_url_.empty()) throw std::invalid_argument("stun_url is required");
    }
};


#endif //MORZE_CONNECTIONPROFILEDAO_H

//
// Created by ivan on 17.04.2026.
//

#ifndef MORZE_CONNECTIONPROFILEDAO_H
#define MORZE_CONNECTIONPROFILEDAO_H
#include <stdexcept>
#include <string>


class ConnectionProfileDAO {
public:
    ConnectionProfileDAO() = default;

    ConnectionProfileDAO(std::string id,
                      std::string server_url,
                      std::string stun_url,
                      std::string updated_at)
        : id_(std::move(id))
        , server_url_(std::move(server_url))
        , stun_url_(std::move(stun_url))
        , updated_at_(std::move(updated_at))
    {
        validateInvariants();
    }

    const std::string& getId() const { return id_; }
    const std::string& getServerUrl() const { return server_url_; }
    const std::string& getStunUrl() const { return stun_url_; }
    const std::string& getUpdatedAt() const { return updated_at_; }

    void setId(std::string id) {
        if (id.empty()) throw std::invalid_argument("id cannot be empty");
        id_ = std::move(id);
    }
    void setServerUrl(std::string url) {
        if (url.empty()) throw std::invalid_argument("server_url cannot be empty");
        server_url_ = std::move(url);
    }
    void setStunUrl(std::string url) {
        stun_url_ = std::move(url);
    }
    void setUpdatedAt(std::string updated_at) {
        if (updated_at.empty()) throw std::invalid_argument("Timestamp cannot be empty");
        updated_at_ = std::move(updated_at);
    }

private:
    std::string id_;
    std::string server_url_;
    std::string stun_url_;
    std::string updated_at_;

    void validateInvariants() const {
        if (id_.empty()) throw std::invalid_argument("id is required");
        if (server_url_.empty()) throw std::invalid_argument("server_url is required");
        if (updated_at_.empty()) throw std::invalid_argument("Timestamp cannot be empty");
    }
};


#endif //MORZE_CONNECTIONPROFILEDAO_H
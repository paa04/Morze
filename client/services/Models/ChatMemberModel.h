#ifndef MORZE_CHATMEMBERMODEL_H
#define MORZE_CHATMEMBERMODEL_H

#include <chrono>
#include <optional>
#include <string>
#include <stdexcept>
#include <boost/uuid/uuid.hpp>

#include "UUIDConverter.h"
#include "TimePointConverter.h"

class ChatMemberModel {
public:
    ChatMemberModel() = default;

    ChatMemberModel(const boost::uuids::uuid id,
                  const boost::uuids::uuid chat_id,
                  std::string username,
                  const std::optional<std::chrono::system_clock::time_point> last_online_at) {
        if (username.empty()) throw std::invalid_argument("username is required");
        id_ = id;
        chat_id_ = chat_id;
        username_ = std::move(username);
        last_online_at_ = last_online_at;
    }

    ChatMemberModel(const boost::uuids::uuid chat_id,
                  std::string username,
                  const std::optional<std::chrono::system_clock::time_point> last_online_at)
        : chat_id_(chat_id)
        , username_(std::move(username))
        , last_online_at_(last_online_at)
    {
        if (username_.empty()) throw std::invalid_argument("username is required");
        boost::uuids::random_generator gen;
        id_ = gen();
    }

    // Геттеры
    const boost::uuids::uuid& getId() const { return id_; }
    std::vector<char> getIdAsBLOB() const { return UUIDConverter::toBlob(id_); }
    std::string getIdAsString() const { return UUIDConverter::toString(id_); }

    const boost::uuids::uuid& getChatId() const { return chat_id_; }
    std::vector<char> getChatIdAsBLOB() const { return UUIDConverter::toBlob(chat_id_); }
    std::string getChatIdAsString() const { return UUIDConverter::toString(chat_id_); }

    const std::string& getUsername() const { return username_; }

    const std::optional<std::chrono::system_clock::time_point>& getLastOnlineAt() const { return last_online_at_; }
    std::optional<std::int64_t> getLastOnlineAtAsUnix() const {
        if (last_online_at_.has_value())
            return TimePointConverter::toUnixSeconds(*last_online_at_);
        return std::nullopt;
    }
    std::optional<std::string> getLastOnlineAtAsString() const {
        if (last_online_at_.has_value())
            return TimePointConverter::toIsoString(*last_online_at_);
        return std::nullopt;
    }

    // Сеттеры
    void setId(const boost::uuids::uuid id) { id_ = id; }
    void setIdFromBLOB(const std::vector<char>& blob) { id_ = UUIDConverter::fromBlob(blob); }
    void setIdFromString(const std::string& str) { id_ = UUIDConverter::fromString(str); }

    void setChatId(const boost::uuids::uuid chat_id) { chat_id_ = chat_id; }
    void setChatIdFromBLOB(const std::vector<char>& blob) { chat_id_ = UUIDConverter::fromBlob(blob); }
    void setChatIdFromString(const std::string& str) { chat_id_ = UUIDConverter::fromString(str); }

    void setUsername(std::string username) {
        if (username.empty()) throw std::invalid_argument("username cannot be empty");
        username_ = std::move(username);
    }

    void setLastOnlineAt(const std::optional<std::chrono::system_clock::time_point> last_online_at) {
        last_online_at_ = last_online_at;
    }
    void setLastOnlineAtFromUnix(const std::optional<std::int64_t>& sec) {
        if (sec.has_value())
            last_online_at_ = TimePointConverter::fromUnixSeconds(*sec);
        else
            last_online_at_ = std::nullopt;
    }
    void setLastOnlineAtFromString(const std::optional<std::string>& iso) {
        if (iso.has_value())
            last_online_at_ = TimePointConverter::fromIsoString(*iso);
        else
            last_online_at_ = std::nullopt;
    }

private:
    boost::uuids::uuid id_;
    boost::uuids::uuid chat_id_;
    std::string username_;
    std::optional<std::chrono::system_clock::time_point> last_online_at_;
};

#endif //MORZE_CHATMEMBERMODEL_H
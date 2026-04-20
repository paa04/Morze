#ifndef MORZE_CHATMEMBERDAO_H
#define MORZE_CHATMEMBERDAO_H

#include <chrono>
#include <optional>
#include <string>
#include <stdexcept>
#include <boost/uuid/uuid.hpp>

#include "UUIDConverter.h"
#include "TimePointConverter.h"

class ChatMemberDAO {
public:
    ChatMemberDAO() = default;

    ChatMemberDAO(boost::uuids::uuid chat_id,
                  boost::uuids::uuid user_id,
                  std::string username,
                  std::optional<std::chrono::system_clock::time_point> last_online_at)
        : chat_id_(chat_id), user_id_(user_id), username_(std::move(username)), last_online_at_(last_online_at) {
        if (username_.empty()) throw std::invalid_argument("username is required");
    }

    // Геттеры
    const boost::uuids::uuid& getChatId() const { return chat_id_; }
    std::vector<char> getChatIdAsBLOB() const { return UUIDConverter::toBlob(chat_id_); }
    std::string getChatIdAsString() const { return UUIDConverter::toString(chat_id_); }

    const boost::uuids::uuid& getUserId() const { return user_id_; }
    std::vector<char> getUserIdAsBLOB() const { return UUIDConverter::toBlob(user_id_); }
    std::string getUserIdAsString() const { return UUIDConverter::toString(user_id_); }

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
    void setChatId(boost::uuids::uuid id) { chat_id_ = id; }
    void setChatIdFromBLOB(const std::vector<char>& blob) { chat_id_ = UUIDConverter::fromBlob(blob); }
    void setChatIdFromString(const std::string& str) { chat_id_ = UUIDConverter::fromString(str); }

    void setUserId(boost::uuids::uuid id) { user_id_ = id; }
    void setUserIdFromBLOB(const std::vector<char>& blob) { user_id_ = UUIDConverter::fromBlob(blob); }
    void setUserIdFromString(const std::string& str) { user_id_ = UUIDConverter::fromString(str); }

    void setUsername(std::string username) {
        if (username.empty()) throw std::invalid_argument("username cannot be empty");
        username_ = std::move(username);
    }

    void setLastOnlineAt(std::optional<std::chrono::system_clock::time_point> tp) { last_online_at_ = tp; }
    void setLastOnlineAtFromUnix(const std::optional<std::int64_t>& sec) {
        if (sec.has_value()) last_online_at_ = TimePointConverter::fromUnixSeconds(*sec);
        else last_online_at_ = std::nullopt;
    }
    void setLastOnlineAtFromString(const std::optional<std::string>& iso) {
        if (iso.has_value()) last_online_at_ = TimePointConverter::fromIsoString(*iso);
        else last_online_at_ = std::nullopt;
    }

private:
    boost::uuids::uuid chat_id_;
    boost::uuids::uuid user_id_;
    std::string username_;
    std::optional<std::chrono::system_clock::time_point> last_online_at_;
};

#endif //MORZE_CHATMEMBERDAO_H
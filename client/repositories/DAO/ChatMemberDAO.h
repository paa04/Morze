//
// Created by ivan on 17.04.2026.
//

#ifndef MORZE_CHATMEMBERDAO_H
#define MORZE_CHATMEMBERDAO_H

#include <chrono>
#include <optional>
#include <string>
#include <stdexcept>
#include <boost/uuid/uuid.hpp>

class ChatMemberDAO {
public:
    ChatMemberDAO() = default;

    ChatMemberDAO(const boost::uuids::uuid &id,
                  const boost::uuids::uuid &chat_id,
                  std::optional<boost::uuids::uuid> member_id,
                  std::optional<boost::uuids::uuid> peer_id,
                  const std::string &username,
                  std::optional<std::chrono::system_clock::time_point> last_online_at) {
        if (username.empty()) throw std::invalid_argument("username is required");
        chat_id_ = chat_id;
        member_id_ = std::move(member_id);
        peer_id_ = std::move(peer_id);
        username_ = username;
        last_online_at_ = last_online_at;
    }

    // Геттеры
    const boost::uuids::uuid &getId() const { return id_; }
    const boost::uuids::uuid &getChatId() const { return chat_id_; }
    const std::optional<boost::uuids::uuid> &getMemberId() const { return member_id_; }
    const std::optional<boost::uuids::uuid> &getPeerId() const { return peer_id_; }
    const std::string &getUsername() const { return username_; }
    const std::optional<std::chrono::system_clock::time_point> &getLastOnlineAt() const { return last_online_at_; }

    // Сеттеры
    void setId(boost::uuids::uuid id) {
        id_ = std::move(id);
    }

    void setChatId(boost::uuids::uuid chat_id) {
        chat_id_ = std::move(chat_id);
    }

    void setMemberId(std::optional<boost::uuids::uuid> member_id) { member_id_ = std::move(member_id); }
    void setPeerId(std::optional<boost::uuids::uuid> peer_id) { peer_id_ = std::move(peer_id); }

    void setUsername(std::string username) {
        if (username.empty()) throw std::invalid_argument("username cannot be empty");
        username_ = std::move(username);
    }

    void setLastOnlineAt(std::optional<std::chrono::system_clock::time_point> last_online_at) {
        last_online_at_ = last_online_at;
    }

private:
    boost::uuids::uuid id_;
    boost::uuids::uuid chat_id_;
    std::optional<boost::uuids::uuid> member_id_;
    std::optional<boost::uuids::uuid> peer_id_;
    std::string username_;
    std::optional<std::chrono::system_clock::time_point> last_online_at_;
};


#endif //MORZE_CHATMEMBERDAO_H

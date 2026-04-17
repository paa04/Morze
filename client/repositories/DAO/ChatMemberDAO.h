//
// Created by ivan on 17.04.2026.
//

#ifndef MORZE_CHATMEMBERDAO_H
#define MORZE_CHATMEMBERDAO_H

#include <optional>
#include<string>
#include<stdexcept>

class ChatMemberDAO {
    class ChatMember {
    public:
        ChatMember() = default;

        ChatMember(std::string id,
                   std::string chat_id,
                   std::optional<std::string> member_id,
                   std::optional<std::string> peer_id,
                   std::string username,
                   std::optional<std::string> last_online_at){
            if (id.empty()) throw std::invalid_argument("id is required");
            if (chat_id.empty()) throw std::invalid_argument("chat_id is required");
            if (username.empty()) throw std::invalid_argument("username is required");
            if (last_online_at.has_value() && last_online_at.value().empty()) throw std::invalid_argument("Timestamp cannot be empty");
            chat_id_ = std::move(chat_id);
            member_id_ = std::move(member_id);
            peer_id_ = std::move(peer_id);
            username_ = std::move(username);
            last_online_at_ = std::move(last_online_at);
        }

        // Геттеры
        const std::string &getId() const { return id_; }
        const std::string &getChatId() const { return chat_id_; }
        const std::optional<std::string> &getMemberId() const { return member_id_; }
        const std::optional<std::string> &getPeerId() const { return peer_id_; }
        const std::string &getUsername() const { return username_; }
        const std::optional<std::string> &getLastOnlineAt() const { return last_online_at_; }

        // Сеттеры
        void setId(std::string id) {
            if (id.empty()) throw std::invalid_argument("id cannot be empty");
            id_ = std::move(id);
        }

        void setChatId(std::string chat_id) {
            if (chat_id.empty()) throw std::invalid_argument("chat_id cannot be empty");
            chat_id_ = std::move(chat_id);
        }

        void setMemberId(std::optional<std::string> member_id) { member_id_ = std::move(member_id); }
        void setPeerId(std::optional<std::string> peer_id) { peer_id_ = std::move(peer_id); }

        void setUsername(std::string username) {
            if (username.empty()) throw std::invalid_argument("username cannot be empty");
            username_ = std::move(username);
        }

        void setLastOnlineAt(std::optional<std::string> last_online_at) {
            if (last_online_at.has_value() && last_online_at.value().empty()) throw std::invalid_argument("Timestamp cannot be empty");
            last_online_at_ = std::move(last_online_at);
        }

    private:
        std::string id_;
        std::string chat_id_;
        std::optional<std::string> member_id_;
        std::optional<std::string> peer_id_;
        std::string username_;
        std::optional<std::string> last_online_at_;
    };
};


#endif //MORZE_CHATMEMBERDAO_H

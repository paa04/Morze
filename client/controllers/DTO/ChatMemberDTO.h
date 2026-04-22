//
// Created by ivan on 22.04.2026.
//

#ifndef CLIENT_CHATMEMBERDTO_H
#define CLIENT_CHATMEMBERDTO_H

#include <string>

class ChatMemberDTO {
public:
    ChatMemberDTO() = default;

    const std::string& getId() const { return id_; }
    const std::string& getUsername() const { return username_; }
    const std::string& getLastOnlineAt() const { return lastOnlineAt_; }

    void setId(const std::string& id) { id_ = id; }
    void setUsername(const std::string& username) { username_ = username; }
    void setLastOnlineAt(const std::string& lastOnlineAt) { lastOnlineAt_ = lastOnlineAt; }

private:
    std::string id_;
    std::string username_;
    std::string lastOnlineAt_;
};

#endif //CLIENT_CHATMEMBERDTO_H
//
// Created by ivan on 22.04.2026.
//

#ifndef CLIENT_USERDTO_H
#define CLIENT_USERDTO_H

#include <string>

class UserDTO {
public:
    UserDTO() = default;

    const std::string& getLogin() const { return login_; }
    const std::string& getPasswordHash() const { return passwordHash_; }
    const std::string& getMemberId() const { return memberId_; }
    const std::string& getCreatedAt() const { return createdAt_; }
    const std::string& getUpdatedAt() const { return updatedAt_; }

    void setLogin(const std::string& login) { login_ = login; }
    void setPasswordHash(const std::string& hash) { passwordHash_ = hash; }
    void setMemberId(const std::string& memberId) { memberId_ = memberId; }
    void setCreatedAt(const std::string& createdAt) { createdAt_ = createdAt; }
    void setUpdatedAt(const std::string& updatedAt) { updatedAt_ = updatedAt; }

private:
    std::string login_;
    std::string passwordHash_;
    std::string memberId_;
    std::string createdAt_;
    std::string updatedAt_;
};

#endif //CLIENT_USERDTO_H
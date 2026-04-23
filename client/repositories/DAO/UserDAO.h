#ifndef MORZE_USERDAO_H
#define MORZE_USERDAO_H

#include <string>
#include <chrono>
#include <boost/uuid/uuid.hpp>
#include "UUIDConverter.h"
#include "TimePointConverter.h"

class UserDAO {
public:
    UserDAO() = default;

    UserDAO(std::string login,
            std::string password_hash,
            boost::uuids::uuid member_id,
            std::chrono::system_clock::time_point created_at,
            std::chrono::system_clock::time_point updated_at)
        : login_(std::move(login))
        , password_hash_(std::move(password_hash))
        , member_id_(std::move(member_id))
        , created_at_(created_at)
        , updated_at_(updated_at) {}

    // Геттеры
    const std::string& getLogin() const { return login_; }
    const std::string& getPasswordHash() const { return password_hash_; }

    const boost::uuids::uuid& getMemberId() const { return member_id_; }
    std::vector<char> getMemberIdAsBLOB() const { return UUIDConverter::toBlob(member_id_); }
    std::string getMemberIdAsString() const { return UUIDConverter::toString(member_id_); }

    const std::chrono::system_clock::time_point& getCreatedAt() const { return created_at_; }
    std::int64_t getCreatedAtAsUnix() const { return TimePointConverter::toUnixSeconds(created_at_); }
    std::string getCreatedAtAsString() const { return TimePointConverter::toIsoString(created_at_); }

    const std::chrono::system_clock::time_point& getUpdatedAt() const { return updated_at_; }
    std::int64_t getUpdatedAtAsUnix() const { return TimePointConverter::toUnixSeconds(updated_at_); }
    std::string getUpdatedAtAsString() const { return TimePointConverter::toIsoString(updated_at_); }

    // Сеттеры
    void setLogin(std::string login) { login_ = std::move(login); }
    void setPasswordHash(std::string hash) { password_hash_ = std::move(hash); }

    void setMemberId(boost::uuids::uuid id) { member_id_ = id; }
    void setMemberIdFromBLOB(const std::vector<char>& blob) { member_id_ = UUIDConverter::fromBlob(blob); }
    void setMemberIdFromString(const std::string& str) { member_id_ = UUIDConverter::fromString(str); }

    void setCreatedAt(std::chrono::system_clock::time_point tp) { created_at_ = tp; }
    void setCreatedAtFromUnix(std::int64_t tp) { created_at_ = TimePointConverter::fromUnixSeconds(tp); }
    void setCreatedAtFromString(const std::string& iso) { created_at_ = TimePointConverter::fromIsoString(iso); }

    void setUpdatedAt(std::chrono::system_clock::time_point tp) { updated_at_ = tp; }
    void setUpdatedAtFromUnix(std::int64_t tp) { updated_at_ = TimePointConverter::fromUnixSeconds(tp); }
    void setUpdatedAtFromString(const std::string& iso) { updated_at_ = TimePointConverter::fromIsoString(iso); }

private:
    std::string login_;
    std::string password_hash_;
    boost::uuids::uuid member_id_;
    std::chrono::system_clock::time_point created_at_;
    std::chrono::system_clock::time_point updated_at_;
};

#endif //MORZE_USERDAO_H
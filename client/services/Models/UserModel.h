#ifndef MORZE_USERMODEL_H
#define MORZE_USERMODEL_H

#include <string>
#include <chrono>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>

class UserModel {
public:
    UserModel() = default;

    UserModel(std::string login,
              std::string password_hash,
              boost::uuids::uuid member_id,
              std::chrono::system_clock::time_point created_at,
              std::chrono::system_clock::time_point updated_at)
        : login_(std::move(login))
        , password_hash_(std::move(password_hash))
        , member_id_(std::move(member_id))
        , created_at_(created_at)
    , updated_at_(updated_at) {}

    UserModel(std::string login,
          std::string password_hash,
          std::chrono::system_clock::time_point created_at,
          std::chrono::system_clock::time_point updated_at)
    : login_(std::move(login))
    , password_hash_(std::move(password_hash))
    , created_at_(created_at)
    , updated_at_(updated_at) {
        boost::uuids::random_generator_mt19937 gen;
        member_id_ = gen();
    }

    const std::string& getLogin() const { return login_; }
    const std::string& getPasswordHash() const { return password_hash_; }
    const boost::uuids::uuid& getMemberId() const { return member_id_; }
    const std::chrono::system_clock::time_point& getCreatedAt() const { return created_at_; }
    const std::chrono::system_clock::time_point& getUpdatedAt() const { return updated_at_; }

    void setLogin(std::string login) { login_ = std::move(login); }
    void setPasswordHash(std::string hash) { password_hash_ = std::move(hash); }
    void setMemberId(boost::uuids::uuid id) { member_id_ = id; }
    void setCreatedAt(std::chrono::system_clock::time_point tp) { created_at_ = tp; }
    void setUpdatedAt(std::chrono::system_clock::time_point tp) { updated_at_ = tp; }

private:
    std::string login_;
    std::string password_hash_;
    boost::uuids::uuid member_id_;
    std::chrono::system_clock::time_point created_at_;
    std::chrono::system_clock::time_point updated_at_;
};

#endif //MORZE_USERMODEL_H
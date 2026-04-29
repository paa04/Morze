#ifndef MORZE_CHATMODEL_H
#define MORZE_CHATMODEL_H

#include <chrono>
#include <stdexcept>
#include <string>
#include <boost/uuid/uuid.hpp>

#include "ChatTypeConverter.h"
#include "GlobalEnums.h"
#include "TimePointConverter.h"
#include "UUIDConverter.h"

class ChatModel {
public:
    ChatModel() = default;

    ChatModel(const boost::uuids::uuid id,
              const boost::uuids::uuid room_id,
              const ChatType type,
              std::string title,
              const std::chrono::system_clock::time_point created_at,
              const int64_t last_message_number) {
        if (title.empty()) throw std::invalid_argument("title is required");

        id_ = id;
        room_id_ = room_id;
        type_ = type;
        title_ = std::move(title);
        created_at_ = created_at;
        last_message_number_ = last_message_number;
    }

    ChatModel(const boost::uuids::uuid room_id,
              const ChatType type,
              std::string title,
              const std::chrono::system_clock::time_point created_at,
              const int64_t last_message_number)
        : room_id_(room_id)
        , type_(type)
        , title_(std::move(title))
        , created_at_(created_at)
        , last_message_number_(last_message_number)
    {
        boost::uuids::random_generator generator;
        id_ = generator();
        if (title_.empty()) throw std::invalid_argument("title is required");
    }

    // Геттеры
    const boost::uuids::uuid& getId() const { return id_; }
    std::vector<char> getIdAsBLOB() const { return UUIDConverter::toBlob(id_); }
    std::string getIdAsString() const { return UUIDConverter::toString(id_); }

    const boost::uuids::uuid& getRoomId() const { return room_id_; }
    std::vector<char> getRoomIdAsBLOB() const { return UUIDConverter::toBlob(room_id_); }
    std::string getRoomIdAsString() const { return UUIDConverter::toString(room_id_); }

    ChatType getType() const { return type_; }
    std::string getTypeAsString() const { return ChatTypeConverter::toString(type_); }

    const std::string& getTitle() const { return title_; }

    const std::chrono::system_clock::time_point& getCreatedAt() const { return created_at_; }
    std::int64_t getCreatedAtAsUnix() const { return TimePointConverter::toUnixSeconds(created_at_); }
    std::string getCreatedAtAsString() const { return TimePointConverter::toIsoString(created_at_); }

    std::int64_t getLastMessageNumber() const { return last_message_number_; }

    // Сеттеры
    void setId(const boost::uuids::uuid id) { id_ = id; }
    void setIdFromBLOB(const std::vector<char>& blob) { id_ = UUIDConverter::fromBlob(blob); }
    void setIdFromString(const std::string& str) { id_ = UUIDConverter::fromString(str); }

    void setRoomId(const boost::uuids::uuid room_id) { room_id_ = room_id; }
    void setRoomIdFromBLOB(const std::vector<char>& blob) { room_id_ = UUIDConverter::fromBlob(blob); }
    void setRoomIdFromString(const std::string& str) { room_id_ = UUIDConverter::fromString(str); }

    void setType(const ChatType type) { type_ = type; }
    void setTypeFromString(const std::string& type) { type_ = ChatTypeConverter::fromString(type); }

    void setTitle(std::string title) {
        if (title.empty()) throw std::invalid_argument("title cannot be empty");
        title_ = std::move(title);
    }

    void setCreatedAt(const std::chrono::system_clock::time_point created_at) { created_at_ = created_at; }
    void setCreatedAtFromString(const std::string& created_at) { created_at_ = TimePointConverter::fromIsoString(created_at); }
    void setCreatedAtFromUnixTime(const std::int64_t created_at) { created_at_ = TimePointConverter::fromUnixSeconds(created_at); }

    void setLastMessageNumber(const std::int64_t last_message_number) { last_message_number_ = last_message_number; }

private:
    boost::uuids::uuid id_;
    boost::uuids::uuid room_id_;
    ChatType type_;
    std::string title_;
    std::chrono::system_clock::time_point created_at_;
    std::int64_t last_message_number_;
};

#endif //MORZE_CHATMODEL_H
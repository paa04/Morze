#ifndef MORZE_CHATDAO_H
#define MORZE_CHATDAO_H

#include <chrono>
#include <stdexcept>
#include <string>
#include <boost/uuid/uuid.hpp>

#include "ChatTypeConverter.h"
#include "GlobalEnums.h"
#include "TimePointConverter.h"
#include "UUIDConverter.h"

class ChatDAO {
public:
    ChatDAO() = default;

    ChatDAO(const boost::uuids::uuid chat_id,
            const boost::uuids::uuid room_id,
            const ChatType type,
            std::string title,
            const std::chrono::system_clock::time_point created_at,
            const std::int64_t last_message_number) {
        if (title.empty()) throw std::invalid_argument("title is required");

        chat_id_ = chat_id;
        room_id_ = room_id;
        type_ = type;
        title_ = std::move(title);
        created_at_ = created_at;
        last_message_number_ = last_message_number;
    }

    // Геттеры
    const boost::uuids::uuid &getChatId() const { return chat_id_; }
    std::vector<char> getChatIdAsBLOB() const { return UUIDConverter::toBlob(chat_id_); }
    std::string getChatIdAsString() const { return UUIDConverter::toString(chat_id_); }
    const boost::uuids::uuid &getRoomId() const { return room_id_; }
    std::vector<char> getRoomIdAsBLOB() const { return UUIDConverter::toBlob(room_id_); }
    std::string getRoomIdAsString() const { return UUIDConverter::toString(room_id_); }
    ChatType getType() const { return type_; }
    std::string getTypeAsString() const { return ChatTypeConverter::toString(type_); }
    const std::string &getTitle() const { return title_; }
    const std::chrono::system_clock::time_point &getCreatedAt() const { return created_at_; }
    std::int64_t getCreatedAtAsUnix() const { return TimePointConverter::toUnixSeconds(created_at_); }
    std::string getCreatedAtAsString() const { return TimePointConverter::toIsoString(created_at_); }
    std::int64_t getLastMessageNumber() const { return last_message_number_; }

    // Сеттеры с валидацией
    void setChatId(const boost::uuids::uuid chat_id) { chat_id_ = chat_id; }
    void setChatIdFromBLOB(const std::vector<char> &chat_id) { chat_id_ = UUIDConverter::fromBlob(chat_id); }
    void setChatIdFromString(const std::string &chat_id) { chat_id_ = UUIDConverter::fromString(chat_id); }

    void setRoomId(const boost::uuids::uuid room_id) { room_id_ = room_id; }
    void setRoomIdFromBLOB(const std::vector<char> &room_id) { room_id_ = UUIDConverter::fromBlob(room_id); }
    void setRoomIdFromString(const std::string &room_id) { room_id_ = UUIDConverter::fromString(room_id); }

    void setType(const ChatType type) { type_ = type; }
    void setTypeFromString(const std::string &type) { type_ = ChatTypeConverter::fromString(type); }

    void setTitle(std::string title) {
        if (title.empty()) throw std::invalid_argument("title cannot be empty");
        title_ = std::move(title);
    }

    void setCreatedAt(const std::chrono::system_clock::time_point created_at) { created_at_ = created_at; }

    void setCreatedAtFromString(const std::string &created_at) {
        created_at_ = TimePointConverter::fromIsoString(created_at);
    }

    void setCreatedAtFromUnixTime(const std::int64_t created_at) {
        created_at_ = TimePointConverter::fromUnixSeconds(created_at);
    }

    void setLastMessageNumber(const std::int64_t last_message_number) { last_message_number_ = last_message_number; }

private:
    boost::uuids::uuid chat_id_;
    boost::uuids::uuid room_id_;
    ChatType type_;
    std::string title_;
    std::chrono::system_clock::time_point created_at_;
    std::int64_t last_message_number_;
};

#endif //MORZE_CHATDAO_H

#ifndef MORZE_CHAT_H
#define MORZE_CHAT_H

#include <chrono>
#include <stdexcept>
#include <string>
#include <boost/uuid/uuid.hpp>

#include "ChatTypeConverter.h"
#include "GlobalEnums.h"

class ChatDAO {
public:
    ChatDAO() = default; // требуется для sqlite_orm (дефолтный конструктор)

    ChatDAO(boost::uuids::uuid chat_id,
            boost::uuids::uuid room_id,
            ChatType type,
            std::string title,
            std::chrono::system_clock::time_point created_at) {
        if (title.empty()) throw std::invalid_argument("title is required");

        chat_id_ = std::move(chat_id);
        room_id_ = std::move(room_id);
        type_ = type;
        title_ = std::move(title);
        created_at_ = created_at;
    }

    // Геттеры
    const boost::uuids::uuid &getChatId() const { return chat_id_; }
    const boost::uuids::uuid &getRoomId() const { return room_id_; }
    const ChatType &getType() const { return type_; }

    const std::string &getTypeAsString() const {
        return std::move(ChatTypeConverter::toString(type_));
    }

    const std::string &getTitle() const { return title_; }
    const std::chrono::system_clock::time_point &getCreatedAt() const { return created_at_; }

    // Сеттеры с валидацией
    void setChatId(boost::uuids::uuid chat_id) {
        chat_id_ = std::move(chat_id);
    }

    void setRoomId(boost::uuids::uuid room_id) {
        room_id_ = std::move(room_id);
    }

    void setType(ChatType type) {
        type_ = type;
    }

    void setTypeFromString(const std::string &type) {
        type_ = ChatTypeConverter::fromString(type);
    }

    void setTitle(std::string title) {
        if (title.empty()) throw std::invalid_argument("title cannot be empty");
        title_ = std::move(title);
    }

    void setCreatedAt(std::chrono::system_clock::time_point created_at) {
        created_at_ = created_at;
    }

private:
    boost::uuids::uuid chat_id_;
    boost::uuids::uuid room_id_;
    ChatType type_;
    std::string title_;
    std::chrono::system_clock::time_point created_at_;
};

#endif //MORZE_CHAT_H

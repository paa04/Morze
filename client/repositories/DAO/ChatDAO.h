#ifndef MORZE_CHAT_H
#define MORZE_CHAT_H

#include <stdexcept>
#include <string>

#include "GlobalEnums.h"

class ChatDAO {
public:
    ChatDAO() = default; // требуется для sqlite_orm (дефолтный конструктор)

    ChatDAO(std::string chat_id,
            std::string room_id,
            ChatType type,
            std::string title,
            std::string created_at) {
        if (chat_id.empty()) throw std::invalid_argument("chat_id is required");
        if (room_id.empty()) throw std::invalid_argument("room_id is required");
        if (title.empty()) throw std::invalid_argument("title is required");

        if (created_at.empty()) throw std::invalid_argument("Timestamp cannot be empty");
        chat_id_ = std::move(chat_id);
        room_id_ = std::move(room_id);
        type_ = type;
        title_ = std::move(title);
        created_at_ = std::move(created_at);
    }

    // Геттеры
    const std::string &getChatId() const { return chat_id_; }
    const std::string &getRoomId() const { return room_id_; }
    const ChatType &getType() const { return type_; }

    const std::string &getTypeAsString() const {
        if (type_ == ChatType::Direct)
            return "direct";
        return "group";
    }

    const std::string &getTitle() const { return title_; }
    const std::string &getCreatedAt() const { return created_at_; }

    // Сеттеры с валидацией
    void setChatId(std::string chat_id) {
        if (chat_id.empty()) throw std::invalid_argument("chat_id cannot be empty");
        chat_id_ = std::move(chat_id);
    }

    void setRoomId(std::string room_id) {
        if (room_id.empty()) throw std::invalid_argument("room_id cannot be empty");
        room_id_ = std::move(room_id);
    }

    void setType(ChatType type) {
        type_ = type;
    }

    void setTypeFromString(std::string type) {
        if (type == "direct") type_ = ChatType::Direct;
        else if (type == "group") type_ = ChatType::Group;
        else throw std::invalid_argument("invalid type");
    }

    void setTitle(std::string title) {
        if (title.empty()) throw std::invalid_argument("title cannot be empty");
        title_ = std::move(title);
    }

    void setCreatedAt(std::string created_at) {
        if (created_at.empty()) throw std::invalid_argument("Timestamp cannot be empty");
        created_at_ = std::move(created_at);
    }

private:
    std::string chat_id_;
    std::string room_id_;
    ChatType type_;
    std::string title_;
    std::string created_at_;
};

#endif //MORZE_CHAT_H

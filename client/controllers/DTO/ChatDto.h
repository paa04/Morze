//
// Created by ivan on 22.04.2026.
//

#ifndef CLIENT_CHATDTO_H
#define CLIENT_CHATDTO_H
#include <string>

class ChatDTO {
public:
    ChatDTO() = default;

    // Геттеры
    const std::string& getId() const { return id_; }
    const std::string& getRoomId() const { return roomId_; }
    const std::string& getType() const { return type_; }
    const std::string& getTitle() const { return title_; }
    const std::string& getCreatedAt() const { return createdAt_; }
    const std::string& getLastMessageNumber() const { return lastMessageNumber_; }

    // Сеттеры
    void setId(const std::string& id) { id_ = id; }
    void setRoomId(const std::string& roomId) { roomId_ = roomId; }
    void setType(const std::string& type) { type_ = type; }
    void setTitle(const std::string& title) { title_ = title; }
    void setCreatedAt(const std::string& createdAt) { createdAt_ = createdAt; }
    void setLastMessageNumber(const std::string& lastMsgNum) { lastMessageNumber_ = lastMsgNum; }

private:
    std::string id_;
    std::string roomId_;
    std::string type_;
    std::string title_;
    std::string createdAt_;
    std::string lastMessageNumber_;
};



#endif //CLIENT_CHATDTO_H
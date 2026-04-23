//
// Created by ivan on 22.04.2026.
//

#ifndef CLIENT_MESSAGEDTO_H
#define CLIENT_MESSAGEDTO_H

#include <string>

class MessageDTO {
public:
    MessageDTO() = default;

    MessageDTO(std::string id, std::string chatId, std::string senderId, std::string direction,
               std::string content, std::string createdAt, std::string deliveryState)
        : id_(std::move(id)), chatId_(std::move(chatId)), senderId_(std::move(senderId)),
          direction_(std::move(direction)), content_(std::move(content)), createdAt_(std::move(createdAt)),
          deliveryState_(std::move(deliveryState)) {
    }

    const std::string &getId() const { return id_; }
    const std::string &getChatId() const { return chatId_; }
    const std::string &getSenderId() const { return senderId_; }
    const std::string &getDirection() const { return direction_; }
    const std::string &getContent() const { return content_; }
    const std::string &getCreatedAt() const { return createdAt_; }
    const std::string &getDeliveryState() const { return deliveryState_; }

    void setId(const std::string &id) { id_ = id; }
    void setChatId(const std::string &chatId) { chatId_ = chatId; }
    void setSenderId(const std::string &senderId) { senderId_ = senderId; }
    void setDirection(const std::string &direction) { direction_ = direction; }
    void setContent(const std::string &content) { content_ = content; }
    void setCreatedAt(const std::string &createdAt) { createdAt_ = createdAt; }
    void setDeliveryState(const std::string &state) { deliveryState_ = state; }

private:
    std::string id_;
    std::string chatId_;
    std::string senderId_;
    std::string direction_;
    std::string content_;
    std::string createdAt_;
    std::string deliveryState_;
};


#endif //CLIENT_MESSAGEDTO_H

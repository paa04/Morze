#ifndef MORZE_MESSAGE_H
#define MORZE_MESSAGE_H
#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>

#include "GlobalEnums.h"


class MessageDAO {
public:
    MessageDAO() = default;

    MessageDAO(const std::string &message_id,
            const std::string &chat_id,
            const std::string &sender_name,
            const MessageDirection &direction,
            const std::string &content,
            const std::string &created_at,
            const DeliveryState &delivery_state)
        : message_id_(message_id)
          , chat_id_(chat_id)
          , sender_name_(sender_name)
          , direction_(direction)
          , content_(content)
          , created_at_(created_at)
          , delivery_state_(delivery_state) {
        validateInvariants();
    }

    // Геттеры
    const std::string &getMessageId() const { return message_id_; }
    const std::string &getChatId() const { return chat_id_; }
    const std::string &getSenderName() const { return sender_name_; }
    const MessageDirection &getDirection() const { return direction_; }

    const std::string &getDirectionAsString() const {
        return direction_ == MessageDirection::Incoming ? "incoming" : "outgoing";
    }

    const std::string &getContent() const { return content_; }
    const std::string &getCreatedAt() const { return created_at_; }
    const DeliveryState &getDeliveryState() const { return delivery_state_; }

    const std::string &getDeliveryStateAsString() const {
        switch (delivery_state_) {
            case DeliveryState::Delivered:  return "delivered";
            case DeliveryState::Failed:     return "failed";
            case DeliveryState::Pending:    return "pending";
            case DeliveryState::Read:       return "read";
            case DeliveryState::Sent:       return "sent";
        }
    }

    // Сеттеры
    void setMessageId(std::string message_id) {
        if (message_id.empty()) throw std::invalid_argument("message_id cannot be empty");
        message_id_ = std::move(message_id);
    }

    void setChatId(std::string chat_id) {
        if (chat_id.empty()) throw std::invalid_argument("chat_id cannot be empty");
        chat_id_ = std::move(chat_id);
    }

    void setSenderName(std::string sender_name) {
        if (sender_name.empty()) throw std::invalid_argument("sender_name cannot be empty");
        sender_name_ = std::move(sender_name);
    }

    void setDirection(MessageDirection direction) {
        direction_ = direction;
    }

    void setDirectionFromString(const std::string &direction) {
        if (direction == "incoming") direction_ = MessageDirection::Incoming;
        else if (direction == "outgoing") direction_ = MessageDirection::Outgoing;
        else throw std::invalid_argument("invalid direction");
    }

    void setContent(std::string content) {
        if (content.empty()) throw std::invalid_argument("content cannot be empty");
        content_ = std::move(content);
    }

    void setCreatedAt(std::string created_at) {
        if (created_at.empty()) throw std::invalid_argument("Timestamp cannot be empty");
        created_at_ = std::move(created_at);
    }

    void setDeliveryState(DeliveryState state) {
        delivery_state_ = state;
    }

    void setDeliveryStateFromString(const std::string &delivery_state) {
        if (delivery_state == "pending") delivery_state_ = DeliveryState::Pending;
        else if (delivery_state == "sent") delivery_state_ = DeliveryState::Sent;
        else if (delivery_state == "delivered") delivery_state_ = DeliveryState::Delivered;
        else if (delivery_state == "read") delivery_state_ = DeliveryState::Read;
        else if (delivery_state == "failed") delivery_state_ = DeliveryState::Failed;
        else throw std::invalid_argument("invalid delivery state");
    }

private:
    std::string message_id_;
    std::string chat_id_;
    std::string sender_name_;
    MessageDirection direction_ = MessageDirection::Outgoing;
    std::string content_;
    std::string created_at_;
    DeliveryState delivery_state_ = DeliveryState::Pending;

    void validateInvariants() const {
        if (message_id_.empty()) throw std::invalid_argument("message_id is required");
        if (chat_id_.empty()) throw std::invalid_argument("chat_id is required");
        if (sender_name_.empty()) throw std::invalid_argument("sender_name is required");
        if (content_.empty()) throw std::invalid_argument("content cannot be empty");
        if (created_at_.empty()) throw std::invalid_argument("Timestamp cannot be empty");
    }
};


#endif //MORZE_MESSAGE_H

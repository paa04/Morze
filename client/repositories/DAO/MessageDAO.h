#ifndef MORZE_MESSAGE_H
#define MORZE_MESSAGE_H
#include <algorithm>
#include <stdexcept>
#include <string>
#include <boost/uuid/uuid.hpp>

#include "DeliveryStateConverter.h"
#include "GlobalEnums.h"
#include "MessageDirectionConverter.h"


class MessageDAO {
public:
    MessageDAO() = default;

    MessageDAO(const boost::uuids::uuid &message_id,
            const boost::uuids::uuid &chat_id,
            const std::string &sender_name,
            const MessageDirection &direction,
            const std::string &content,
            const std::chrono::system_clock::time_point &created_at,
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
    const boost::uuids::uuid &getMessageId() const { return message_id_; }
    const boost::uuids::uuid &getChatId() const { return chat_id_; }
    const std::string &getSenderName() const { return sender_name_; }
    const MessageDirection &getDirection() const { return direction_; }

    const std::string &getDirectionAsString() const {
        return std::move(MessageDirectionConverter::toString(direction_));
    }

    const std::string &getContent() const { return content_; }
    const std::chrono::system_clock::time_point &getCreatedAt() const { return created_at_; }
    const DeliveryState &getDeliveryState() const { return delivery_state_; }

    const std::string &getDeliveryStateAsString() const {
        return std::move(DeliveryStateConverter::toString(delivery_state_));
    }

    // Сеттеры
    void setMessageId(boost::uuids::uuid message_id) {
        message_id_ = std::move(message_id);
    }

    void setChatId(boost::uuids::uuid chat_id) {
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
        direction_ = MessageDirectionConverter::fromString(direction);
    }

    void setContent(std::string content) {
        if (content.empty()) throw std::invalid_argument("content cannot be empty");
        content_ = std::move(content);
    }

    void setCreatedAt(std::chrono::system_clock::time_point created_at) {
        created_at_ = created_at;
    }

    void setDeliveryState(DeliveryState state) {
        delivery_state_ = state;
    }

    void setDeliveryStateFromString(const std::string &delivery_state) {
            delivery_state_ = DeliveryStateConverter::fromString(delivery_state);
    }

private:
    boost::uuids::uuid message_id_;
    boost::uuids::uuid chat_id_;
    std::string sender_name_;
    MessageDirection direction_ = MessageDirection::Outgoing;
    std::string content_;
    std::chrono::system_clock::time_point created_at_;
    DeliveryState delivery_state_ = DeliveryState::Pending;

    void validateInvariants() const {
        if (sender_name_.empty()) throw std::invalid_argument("sender_name is required");
        if (content_.empty()) throw std::invalid_argument("content cannot be empty");
    }
};


#endif //MORZE_MESSAGE_H

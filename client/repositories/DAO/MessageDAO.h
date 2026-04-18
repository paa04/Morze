#ifndef MORZE_MESSAGEDAO_H
#define MORZE_MESSAGEDAO_H

#include <algorithm>
#include <stdexcept>
#include <string>
#include <boost/uuid/uuid.hpp>

#include "DeliveryStateConverter.h"
#include "GlobalEnums.h"
#include "MessageDirectionConverter.h"
#include "TimePointConverter.h"
#include "UUIDConverter.h"

class MessageDAO {
public:
    MessageDAO() = default;

    MessageDAO(const boost::uuids::uuid message_id,
               const boost::uuids::uuid chat_id,
               const boost::uuids::uuid sender_id,
               const MessageDirection direction,
               std::string content,
               const std::chrono::system_clock::time_point created_at,
               const DeliveryState delivery_state)
        : message_id_(message_id)
        , chat_id_(chat_id)
        , sender_id_(sender_id)
        , direction_(direction)
        , content_(std::move(content))
        , created_at_(created_at)
        , delivery_state_(delivery_state) {
        validateInvariants();
    }

    // Геттеры
    const boost::uuids::uuid& getMessageId() const { return message_id_; }
    std::vector<char> getMessageIdAsBLOB() const { return UUIDConverter::toBlob(message_id_); }
    std::string getMessageIdAsString() const { return UUIDConverter::toString(message_id_); }

    const boost::uuids::uuid& getChatId() const { return chat_id_; }
    std::vector<char> getChatIdAsBLOB() const { return UUIDConverter::toBlob(chat_id_); }
    std::string getChatIdAsString() const { return UUIDConverter::toString(chat_id_); }

    const boost::uuids::uuid& getSenderId() const { return sender_id_; }
    std::vector<char> getSenderIdAsBLOB() const { return UUIDConverter::toBlob(sender_id_); }
    std::string getSenderIdAsString() const { return UUIDConverter::toString(sender_id_); }

    MessageDirection getDirection() const { return direction_; }
    std::string getDirectionAsString() const { return MessageDirectionConverter::toString(direction_); }

    const std::string& getContent() const { return content_; }

    const std::chrono::system_clock::time_point& getCreatedAt() const { return created_at_; }
    std::int64_t getCreatedAtAsUnixTime() const { return TimePointConverter::toUnixSeconds(created_at_); }
    std::string getCreatedAtAsString() const { return TimePointConverter::toIsoString(created_at_); }

    DeliveryState getDeliveryState() const { return delivery_state_; }
    std::string getDeliveryStateAsString() const { return DeliveryStateConverter::toString(delivery_state_); }

    // Сеттеры
    void setMessageId(const boost::uuids::uuid message_id) { message_id_ = message_id; }
    void setMessageIdFromBLOB(const std::vector<char>& blob) { message_id_ = UUIDConverter::fromBlob(blob); }
    void setMessageIdFromString(const std::string& str) { message_id_ = UUIDConverter::fromString(str); }

    void setChatId(const boost::uuids::uuid chat_id) { chat_id_ = chat_id; }
    void setChatIdFromBLOB(const std::vector<char>& blob) { chat_id_ = UUIDConverter::fromBlob(blob); }
    void setChatIdFromString(const std::string& str) { chat_id_ = UUIDConverter::fromString(str); }

    void setSenderId(const boost::uuids::uuid chat_id) { sender_id_ = chat_id; }
    void setSenderIdFromBLOB(const std::vector<char>& blob) { sender_id_ = UUIDConverter::fromBlob(blob); }
    void setSenderIdFromString(const std::string& str) { sender_id_ = UUIDConverter::fromString(str); }

    void setDirection(const MessageDirection direction) { direction_ = direction; }
    void setDirectionFromString(const std::string& str) { direction_ = MessageDirectionConverter::fromString(str); }

    void setContent(std::string content) {
        if (content.empty()) throw std::invalid_argument("content cannot be empty");
        content_ = std::move(content);
    }

    void setCreatedAt(const std::chrono::system_clock::time_point created_at) { created_at_ = created_at; }
    void setCreatedAtFromUnixTime(const std::int64_t created_at) { created_at_ = TimePointConverter::fromUnixSeconds(created_at); }
    void setCreatedAtFromString(const std::string& iso) { created_at_ = TimePointConverter::fromIsoString(iso); }

    void setDeliveryState(const DeliveryState state) { delivery_state_ = state; }
    void setDeliveryStateFromString(const std::string& str) { delivery_state_ = DeliveryStateConverter::fromString(str); }

private:
    boost::uuids::uuid message_id_;
    boost::uuids::uuid chat_id_;
    boost::uuids::uuid sender_id_;
    MessageDirection direction_ = MessageDirection::Outgoing;
    std::string content_;
    std::chrono::system_clock::time_point created_at_;
    DeliveryState delivery_state_ = DeliveryState::Pending;

    void validateInvariants() const {
        if (content_.empty()) throw std::invalid_argument("content cannot be empty");
    }
};

#endif //MORZE_MESSAGEDAO_H
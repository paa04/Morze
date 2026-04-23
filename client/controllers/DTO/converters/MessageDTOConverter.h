#ifndef MORZE_MESSAGEDTOCONVERTER_H
#define MORZE_MESSAGEDTOCONVERTER_H

#include "MessageModel.h"
#include "MessageDTO.h"
#include "UUIDConverter.h"
#include "TimePointConverter.h"
#include "MessageDirectionConverter.h"
#include "DeliveryStateConverter.h"

class MessageDTOConverter {
public:
    static MessageDTO toDto(const MessageModel& model) {
        MessageDTO dto;
        dto.setId(UUIDConverter::toString(model.getMessageId()));
        dto.setChatId(UUIDConverter::toString(model.getChatId()));
        dto.setSenderId(UUIDConverter::toString(model.getSenderId()));
        dto.setDirection(MessageDirectionConverter::toString(model.getDirection()));
        dto.setContent(model.getContent());
        dto.setCreatedAt(TimePointConverter::toIsoString(model.getCreatedAt()));
        dto.setDeliveryState(DeliveryStateConverter::toString(model.getDeliveryState()));
        return dto;
    }

    static MessageModel fromDto(const MessageDTO& dto) {
        return MessageModel(
            UUIDConverter::fromString(dto.getId()),
            UUIDConverter::fromString(dto.getChatId()),
            UUIDConverter::fromString(dto.getSenderId()),
            MessageDirectionConverter::fromString(dto.getDirection()),
            dto.getContent(),
            TimePointConverter::fromIsoString(dto.getCreatedAt()),
            DeliveryStateConverter::fromString(dto.getDeliveryState())
        );
    }
};

#endif // MORZE_MESSAGEDTOCONVERTER_H
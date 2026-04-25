#ifndef MORZE_CHATDTOCONVERTER_H
#define MORZE_CHATDTOCONVERTER_H

#include "ChatModel.h"
#include "ChatDTO.h"
#include "UUIDConverter.h"
#include "TimePointConverter.h"
#include "ChatTypeConverter.h"

class ChatDTOConverter {
public:
    static ChatDTO toDto(const ChatModel& model) {
        ChatDTO dto;
        dto.setId(UUIDConverter::toString(model.getId()));
        dto.setRoomId(UUIDConverter::toString(model.getRoomId()));
        dto.setType(ChatTypeConverter::toString(model.getType()));
        dto.setTitle(model.getTitle());
        dto.setCreatedAt(TimePointConverter::toIsoString(model.getCreatedAt()));
        dto.setLastMessageNumber(std::to_string(model.getLastMessageNumber()));
        return dto;
    }

    static ChatModel fromDto(const ChatDTO& dto) {
        return ChatModel(
            UUIDConverter::fromString(dto.getId()),
            UUIDConverter::fromString(dto.getRoomId()),
            ChatTypeConverter::fromString(dto.getType()),
            dto.getTitle(),
            TimePointConverter::fromIsoString(dto.getCreatedAt()),
            std::stoll(dto.getLastMessageNumber())
        );
    }
};

#endif // MORZE_CHATDTOCONVERTER_H
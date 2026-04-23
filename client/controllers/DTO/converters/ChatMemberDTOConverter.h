#ifndef MORZE_CHATMEMBERDTOCONVERTER_H
#define MORZE_CHATMEMBERDTOCONVERTER_H

#include "ChatMemberModel.h"
#include "ChatMemberDTO.h"
#include "UUIDConverter.h"
#include "TimePointConverter.h"

class ChatMemberDTOConverter {
public:
    static ChatMemberDTO toDto(const ChatMemberModel& model) {
        ChatMemberDTO dto;
        dto.setId(UUIDConverter::toString(model.getId()));
        dto.setUsername(model.getUsername());
        if (model.getLastOnlineAt().has_value())
            dto.setLastOnlineAt(TimePointConverter::toIsoString(*model.getLastOnlineAt()));
        else
            dto.setLastOnlineAt("");
        return dto;
    }

    static ChatMemberModel fromDto(const ChatMemberDTO& dto) {
        std::optional<std::chrono::system_clock::time_point> lastOnline;
        if (!dto.getLastOnlineAt().empty())
            lastOnline = TimePointConverter::fromIsoString(dto.getLastOnlineAt());
        return ChatMemberModel(
            UUIDConverter::fromString(dto.getId()),
            dto.getUsername(),
            lastOnline
        );
    }
};

#endif // MORZE_CHATMEMBERDTOCONVERTER_H
#ifndef MORZE_USERDTOCONVERTER_H
#define MORZE_USERDTOCONVERTER_H

#include "UserModel.h"
#include "UserDTO.h"
#include "UUIDConverter.h"
#include "TimePointConverter.h"

class UserDTOConverter {
public:
    static UserDTO toDto(const UserModel& model) {
        UserDTO dto;
        dto.setLogin(model.getLogin());
        dto.setPasswordHash(model.getPasswordHash());
        dto.setMemberId(UUIDConverter::toString(model.getMemberId()));
        dto.setCreatedAt(TimePointConverter::toIsoString(model.getCreatedAt()));
        dto.setUpdatedAt(TimePointConverter::toIsoString(model.getUpdatedAt()));
        return dto;
    }

    static UserModel fromDto(const UserDTO& dto) {
        return UserModel(
            dto.getLogin(),
            dto.getPasswordHash(),
            UUIDConverter::fromString(dto.getMemberId()),
            TimePointConverter::fromIsoString(dto.getCreatedAt()),
            TimePointConverter::fromIsoString(dto.getUpdatedAt())
        );
    }
};

#endif // MORZE_USERDTOCONVERTER_H
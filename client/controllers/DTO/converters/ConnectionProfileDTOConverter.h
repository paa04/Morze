#ifndef MORZE_CONNECTIONPROFILEDTOCONVERTER_H
#define MORZE_CONNECTIONPROFILEDTOCONVERTER_H

#include "ConnectionProfileModel.h"
#include "ConnectionProfileDTO.h"
#include "UUIDConverter.h"
#include "TimePointConverter.h"

class ConnectionProfileDTOConverter {
public:
    static ConnectionProfileDTO toDto(const ConnectionProfileModel& model) {
        ConnectionProfileDTO dto;
        dto.setId(UUIDConverter::toString(model.getId()));
        dto.setServerUrl(model.getServerUrl());
        dto.setStunUrl(model.getStunUrl());
        dto.setUpdatedAt(TimePointConverter::toIsoString(model.getUpdatedAt()));
        return dto;
    }

    static ConnectionProfileModel fromDto(const ConnectionProfileDTO& dto) {
        return ConnectionProfileModel(
            UUIDConverter::fromString(dto.getId()),
            dto.getServerUrl(),
            dto.getStunUrl(),
            TimePointConverter::fromIsoString(dto.getUpdatedAt())
        );
    }
};

#endif // MORZE_CONNECTIONPROFILEDTOCONVERTER_H
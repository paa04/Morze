//
// Created by ivan on 18.04.2026.
//

#ifndef MORZE_CHATTYPECONVERTER_H
#define MORZE_CHATTYPECONVERTER_H
#include <string>

#include "GlobalEnums.h"


class ChatTypeConverter {
public:
    static std::string toString(const ChatType chatType) {
        if (chatType == ChatType::Direct) return "direct";
        return "group";
    }

    static ChatType fromString(const std::string &type) {
        if (type == "direct") return ChatType::Direct;
        if (type == "group") return ChatType::Group;
        throw std::invalid_argument("invalid type");
    }
};


#endif //MORZE_CHATTYPECONVERTER_H
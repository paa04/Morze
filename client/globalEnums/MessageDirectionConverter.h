//
// Created by ivan on 18.04.2026.
//

#ifndef MORZE_MESSAGEDIRECTIONCONVERTER_H
#define MORZE_MESSAGEDIRECTIONCONVERTER_H

#include<string>

#include<GlobalEnums.h>

class MessageDirectionConverter {
    public:
    static std::string toString(MessageDirection direction) {
        return direction == MessageDirection::Incoming ? "incoming" : "outgoing";
    }

    static MessageDirection fromString(const std::string &direction) {
        if (direction == "incoming") return MessageDirection::Incoming;
        if (direction == "outgoing") return MessageDirection::Outgoing;
        throw std::invalid_argument("invalid direction");
    }
};


#endif //MORZE_MESSAGEDIRECTIONCONVERTER_H
//
// Created by ivan on 18.04.2026.
//

#ifndef MORZE_DELIVERYSTATECONVERTER_H
#define MORZE_DELIVERYSTATECONVERTER_H
#include <string>

#include "GlobalEnums.h"


class DeliveryStateConverter {
public:
    static std::string toString(const DeliveryState delivery_state) {
        switch (delivery_state) {
            case DeliveryState::Delivered:  return "delivered";
            case DeliveryState::Failed:     return "failed";
            case DeliveryState::Pending:    return "pending";
            case DeliveryState::Read:       return "read";
            case DeliveryState::Sent:       return "sent";
        }
    }

    static DeliveryState fromString(const std::string &delivery_state) {
        if (delivery_state == "pending") return DeliveryState::Pending;
        if (delivery_state == "sent") return DeliveryState::Sent;
        if (delivery_state == "delivered") return  DeliveryState::Delivered;
        if (delivery_state == "read") return  DeliveryState::Read;
        if (delivery_state == "failed") return DeliveryState::Failed;
        throw std::invalid_argument("invalid delivery state");
    }
};


#endif //MORZE_DELIVERYSTATECONVERTER_H
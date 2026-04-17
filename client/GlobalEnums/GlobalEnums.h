//
// Created by ivan on 17.04.2026.
//

#ifndef MORZE_GLOBALENUMS_H
#define MORZE_GLOBALENUMS_H

enum class MessageDirection {
    Incoming,
    Outgoing
};

enum class DeliveryState {
    Pending,
    Sent,
    Delivered,
    Read,
    Failed
};

enum class ChatType {
    Direct,
    Group
};

#endif //MORZE_GLOBALENUMS_H
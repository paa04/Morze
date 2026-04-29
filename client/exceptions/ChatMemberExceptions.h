//
// Created by ivan on 18.04.2026.
//

#ifndef MORZE_CHATMEMBEREXCEPTIONS_H
#define MORZE_CHATMEMBEREXCEPTIONS_H
#include <stdexcept>
#include <string_view>

class ChatMemberNotFoundError : public std::runtime_error {
public:
    explicit ChatMemberNotFoundError(const std::string_view& message)
        : std::runtime_error(message.data()) {}
};

class ChatMemberAlreadyExistsError : public std::runtime_error {
public:
    explicit ChatMemberAlreadyExistsError(const std::string_view& message)
        : std::runtime_error(message.data()) {}
};


#endif //MORZE_CHATMEMBEREXCEPTIONS_H
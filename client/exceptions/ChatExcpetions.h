//
// Created by ivan on 18.04.2026.
//

#ifndef MORZE_CHATEXCPETIONS_H
#define MORZE_CHATEXCPETIONS_H
#include <stdexcept>
#include <string>

class ChatNotFoundError : public std::runtime_error {
public:
    explicit ChatNotFoundError(const std::string_view &message)
        : std::runtime_error(message.data()) {
    }
};

class ChatAlreadyExistsError : public std::runtime_error {
public:
    explicit ChatAlreadyExistsError(const std::string_view &message)
        : std::runtime_error(message.data()) {
    }
};
#endif //MORZE_CHATEXCPETIONS_H

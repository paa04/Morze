//
// Created by ivan on 20.04.2026.
//

#ifndef MORZE_MESSAGEEXCEPTIONS_H
#define MORZE_MESSAGEEXCEPTIONS_H

#include <stdexcept>
#include <string_view>

class MessageNotFoundError : public std::runtime_error {
public:
    explicit MessageNotFoundError(const std::string_view& message)
        : std::runtime_error(message.data()) {}
};

class MessageAlreadyExistsError : public std::runtime_error {
public:
    explicit MessageAlreadyExistsError(const std::string_view& message)
        : std::runtime_error(message.data()) {}
};

#endif //MORZE_MESSAGEEXCEPTIONS_H
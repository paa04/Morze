//
// Created by ivan on 20.04.2026.
//

#ifndef MORZE_CONNECTIONPROFILEEXCEPTIONS_H
#define MORZE_CONNECTIONPROFILEEXCEPTIONS_H

#include <stdexcept>
#include <string_view>

class ConnectionProfileNotFoundError : public std::runtime_error {
public:
    explicit ConnectionProfileNotFoundError(const std::string_view& message)
        : std::runtime_error(message.data()) {}
};

class ConnectionProfileAlreadyExistsError : public std::runtime_error {
public:
    explicit ConnectionProfileAlreadyExistsError(const std::string_view& message)
        : std::runtime_error(message.data()) {}
};

#endif //MORZE_CONNECTIONPROFILEEXCEPTIONS_H
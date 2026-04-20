#ifndef MORZE_USEREXCEPTIONS_H
#define MORZE_USEREXCEPTIONS_H

#include <stdexcept>
#include <string_view>

class UserNotFoundError : public std::runtime_error {
public:
    explicit UserNotFoundError(const std::string_view& msg)
        : std::runtime_error(msg.data()) {}
};

class UserAlreadyExistsError : public std::runtime_error {
public:
    explicit UserAlreadyExistsError(const std::string_view& msg)
        : std::runtime_error(msg.data()) {}
};

#endif //MORZE_USEREXCEPTIONS_H
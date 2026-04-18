//
// Created by ivan on 18.04.2026.
//

#ifndef MORZE_TIMEPOINTCONVERTER_H
#define MORZE_TIMEPOINTCONVERTER_H

#include<chrono>
#include<string>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#ifdef _WIN32
#include <ctime>
#else
#include <ctime>
#endif

namespace detail {
    // Кроссплатформенная замена timegm
    inline std::time_t timegm(std::tm* tm) {
#ifdef _WIN32
        return _mkgmtime(tm);
#else
        return ::timegm(tm);
#endif
    }
}

class TimePointConverter {
public:
    static std::string toIsoString(const std::chrono::system_clock::time_point &tp) {
        std::time_t tt = std::chrono::system_clock::to_time_t(tp);
        std::tm tm = {};
#if defined(_WIN32)
        if (gmtime_s(&tm, &tt) != 0) {
            throw std::runtime_error("Failed to convert time_point to UTC tm structure");
        }
#else
        if (gmtime_r(&tt, &tm) == nullptr) {
            throw std::runtime_error("Failed to convert time_point to UTC tm structure");
        }
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }

    static std::chrono::system_clock::time_point fromIsoString(const std::string &isoStr) {
        std::tm tm = {};
        std::istringstream iss(isoStr);
        iss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        if (iss.fail()) {
            throw std::invalid_argument("Invalid ISO 8601 timestamp: " + isoStr);
        }
        std::time_t tt = detail::timegm(&tm);
        return std::chrono::system_clock::from_time_t(tt);
    }

    // Преобразование std::chrono::system_clock::time_point в Unix-секунды (UTC)
    static std::int64_t toUnixSeconds(const std::chrono::system_clock::time_point &tp) {
        return std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
    }

    // Преобразование Unix-секунд в std::chrono::system_clock::time_point
    static std::chrono::system_clock::time_point fromUnixSeconds(std::int64_t seconds) {
        return std::chrono::system_clock::time_point{std::chrono::seconds{seconds}};
    }
};


#endif

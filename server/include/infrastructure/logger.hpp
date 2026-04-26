#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string_view>

namespace signaling::infrastructure {

enum class LogLevel { INFO, WARN, ERROR };

inline void log(LogLevel level, std::string_view msg) {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&time, &tm);

    const char* tag = "INFO";
    if (level == LogLevel::WARN)  tag = "WARN";
    if (level == LogLevel::ERROR) tag = "ERROR";

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ")
        << " [" << tag << "] " << msg << '\n';

    auto& out = (level == LogLevel::ERROR) ? std::cerr : std::cout;
    out << oss.str() << std::flush;
}

} // namespace signaling::infrastructure

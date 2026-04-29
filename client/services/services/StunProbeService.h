#ifndef STUNPROBESERVICE_H
#define STUNPROBESERVICE_H

#include <optional>
#include <string>
#include <vector>
#include <chrono>
#include <cstdint>

class StunProbeService {
public:
    struct Endpoint {
        std::string ip;
        uint16_t port = 0;
        std::string candidate;
    };

    std::optional<Endpoint> detectPublicEndpoint(
        const std::vector<std::string>& stunServers,
        std::chrono::milliseconds timeout = std::chrono::seconds(5)
    ) const;
};

#endif // STUNPROBESERVICE_H

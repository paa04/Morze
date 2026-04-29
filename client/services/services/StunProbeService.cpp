#include "StunProbeService.h"

#include <rtc/rtc.hpp>

#include <algorithm>
#include <atomic>
#include <future>
#include <sstream>

namespace {
std::optional<StunProbeService::Endpoint> parseSrflxCandidate(const std::string& candidateLine) {
    std::istringstream stream(candidateLine);
    std::vector<std::string> parts;
    std::string part;
    while (stream >> part) {
        parts.push_back(part);
    }

    if (parts.size() < 8) {
        return std::nullopt;
    }

    auto typIt = std::find(parts.begin(), parts.end(), "typ");
    if (typIt == parts.end() || (typIt + 1) == parts.end() || *(typIt + 1) != "srflx") {
        return std::nullopt;
    }

    try {
        StunProbeService::Endpoint endpoint;
        endpoint.ip = parts.at(4);
        endpoint.port = static_cast<uint16_t>(std::stoul(parts.at(5)));
        endpoint.candidate = candidateLine;
        return endpoint;
    } catch (...) {
        return std::nullopt;
    }
}
}

std::optional<StunProbeService::Endpoint> StunProbeService::detectPublicEndpoint(
    const std::vector<std::string>& stunServers,
    std::chrono::milliseconds timeout
) const {
    rtc::Configuration config;
    for (const auto& server : stunServers) {
        config.iceServers.emplace_back(server);
    }

    std::promise<std::optional<Endpoint>> resultPromise;
    auto resultFuture = resultPromise.get_future();
    auto completed = std::make_shared<std::atomic_bool>(false);

    auto completeOnce = [completed, &resultPromise](std::optional<Endpoint> endpoint) {
        if (!completed->exchange(true)) {
            resultPromise.set_value(std::move(endpoint));
        }
    };

    auto pc = std::make_shared<rtc::PeerConnection>(config);
    auto dc = pc->createDataChannel("stun-probe");

    pc->onLocalCandidate([completeOnce](rtc::Candidate cand) {
        const auto parsed = parseSrflxCandidate(cand.candidate());
        if (parsed.has_value()) {
            completeOnce(parsed);
        }
    });

    pc->onGatheringStateChange([completeOnce](rtc::PeerConnection::GatheringState state) {
        if (state == rtc::PeerConnection::GatheringState::Complete) {
            completeOnce(std::nullopt);
        }
    });

    pc->setLocalDescription();
    const auto status = resultFuture.wait_for(timeout);

    pc->close();
    dc.reset();
    pc.reset();

    if (status == std::future_status::ready) {
        return resultFuture.get();
    }

    return std::nullopt;
}

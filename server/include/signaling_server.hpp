#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace signaling {

    class SignalingServer {
    public:
        explicit SignalingServer(uint16_t port, std::size_t threads = 1,
                                std::string dbPath = "", int canaryPercent = 50,
                                bool canaryActive = true);

        ~SignalingServer();

        SignalingServer(const SignalingServer &) = delete;

        SignalingServer &operator=(const SignalingServer &) = delete;

        bool start();

        void run();

        void stop();

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace signaling

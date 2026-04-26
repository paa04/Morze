#include "signaling_server.hpp"
#include "infrastructure/logger.hpp"

#include <boost/asio/signal_set.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <thread>

using signaling::infrastructure::log;
using signaling::infrastructure::LogLevel;

int main(int argc, char *argv[]) {
    uint16_t port = 9001;
    std::size_t threads = 1;
    std::string dbPath = "morze.db";

    try {
        if (argc > 1) {
            port = static_cast<uint16_t>(std::stoi(argv[1]));
        } else if (const char *envPort = std::getenv("PORT")) {
            port = static_cast<uint16_t>(std::stoi(envPort));
        }

        if (argc > 2) {
            threads = std::max<std::size_t>(1, static_cast<std::size_t>(std::stoul(argv[2])));
        } else if (const char *envThreads = std::getenv("THREADS")) {
            threads = std::max<std::size_t>(1, static_cast<std::size_t>(std::stoul(envThreads)));
        }

        if (argc > 3) {
            dbPath = argv[3];
        } else if (const char *envDb = std::getenv("DB_PATH")) {
            dbPath = envDb;
        }
    } catch (const std::exception &ex) {
        log(LogLevel::ERROR, std::string("Invalid startup parameter: ") + ex.what());
        return 1;
    }

    signaling::SignalingServer server(port, threads, dbPath);
    if (!server.start()) {
        return 1;
    }

    // Graceful shutdown on SIGTERM/SIGINT (Factor 9: Disposability)
    boost::asio::io_context signal_ioc;
    boost::asio::signal_set signals(signal_ioc, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code&, int sig) {
        log(LogLevel::INFO, std::string("Received signal ") + std::to_string(sig) + ", shutting down...");
        server.stop();
    });
    std::thread signal_thread([&]() { signal_ioc.run(); });

    server.run();

    signal_ioc.stop();
    if (signal_thread.joinable()) {
        signal_thread.join();
    }

    log(LogLevel::INFO, "Server stopped");
    return 0;
}

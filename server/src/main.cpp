#include "signaling_server.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>

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
        std::cerr << "Invalid startup parameter: " << ex.what() << '\n';
        return 1;
    }

    signaling::SignalingServer server(port, threads, dbPath);
    if (!server.start()) {
        return 1;
    }

    server.run();
    return 0;
}

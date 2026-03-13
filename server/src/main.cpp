#include "signaling_server.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>

int main(int argc, char* argv[]) {
  uint16_t port = 9001;

  try {
    if (argc > 1) {
      port = static_cast<uint16_t>(std::stoi(argv[1]));
    } else if (const char* envPort = std::getenv("PORT")) {
      port = static_cast<uint16_t>(std::stoi(envPort));
    }
  } catch (const std::exception& ex) {
    std::cerr << "Invalid port value: " << ex.what() << '\n';
    return 1;
  }

  signaling::SignalingServer server(port);
  if (!server.start()) {
    return 1;
  }

  server.run();
  return 0;
}

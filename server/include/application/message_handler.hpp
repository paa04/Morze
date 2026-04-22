#pragma once

#include "domain/room_registry.hpp"

#include <memory>
#include <string>

namespace signaling::application {

class MessageHandler {
public:
  explicit MessageHandler(std::shared_ptr<domain::RoomRegistry> registry);

  void handleMessage(const std::shared_ptr<domain::IConnection>& session, const std::string& raw);
  void handleDisconnect(const std::shared_ptr<domain::IConnection>& session);

private:
  std::shared_ptr<domain::RoomRegistry> registry_;
};

} // namespace signaling::application

#pragma once

#include <boost/json.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace signaling {

namespace protocol {

std::optional<boost::json::object> parseObject(std::string_view raw, std::string& error);
std::optional<std::string> getStringField(const boost::json::object& obj, std::string_view key);

boost::json::object makeError(std::string_view message);
boost::json::object makePeers(const std::vector<std::string>& peers);
boost::json::object makePeerJoined(std::string_view peerId);
boost::json::object makePeerLeft(std::string_view peerId);
boost::json::object makeSignalForward(std::string_view fromPeerId, const boost::json::value& data);

} // namespace protocol

class SignalingServer {
public:
  explicit SignalingServer(uint16_t port, std::size_t threads = 1);
  ~SignalingServer();

  SignalingServer(const SignalingServer&) = delete;
  SignalingServer& operator=(const SignalingServer&) = delete;

  bool start();
  void run();
  void stop();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace signaling

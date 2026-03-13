#pragma once

#include <cstdint>
#include <atomic>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace signaling {

namespace protocol {

std::optional<std::string> getJsonStringField(const std::string& json, const std::string& key);
std::optional<std::string> getJsonRawValue(const std::string& json, const std::string& key);
std::string jsonEscape(const std::string& input);
std::string wsAcceptValue(const std::string& wsKey);
bool decodeClientFrame(std::string& buffer, std::string& payload, uint8_t& opcode, bool& isClose);

} // namespace protocol

struct ClientState {
  int fd{-1};
  bool handshaken{false};
  std::string readBuffer;
  std::string roomId;
  std::string peerId;
};

class SignalingServer {
public:
  explicit SignalingServer(uint16_t port);
  ~SignalingServer();

  SignalingServer(const SignalingServer&) = delete;
  SignalingServer& operator=(const SignalingServer&) = delete;

  bool start();
  void run();
  void stop();

private:
  bool setupListenSocket();
  void eventLoopTick();
  void acceptClient();
  void handleClientReadable(int clientFd);
  void disconnectClient(int clientFd);

  bool processHandshake(ClientState& client);
  bool processFrames(ClientState& client);
  bool decodeOneFrame(std::string& buffer, std::string& payload, uint8_t& opcode, bool& isClose);
  bool sendTextFrame(int fd, const std::string& text);

  void handleMessage(ClientState& client, const std::string& message);
  void handleJoin(ClientState& client, const std::string& message);
  void handleSignal(ClientState& client, const std::string& message);
  void leaveRoom(ClientState& client);

  void sendError(int fd, const std::string& message);
  void sendPeers(int fd, const std::vector<std::string>& peers);
  void broadcastPeerJoined(const std::string& roomId, const std::string& peerId, int exceptFd);
  void broadcastPeerLeft(const std::string& roomId, const std::string& peerId, int exceptFd);

  std::optional<std::string> getJsonStringField(const std::string& json, const std::string& key) const;
  std::optional<std::string> getJsonRawValue(const std::string& json, const std::string& key) const;

  static std::string jsonEscape(const std::string& input);
  static std::string wsAcceptValue(const std::string& wsKey);
  static std::string sha1(const std::string& input);
  static std::string base64Encode(const std::string& input);

  uint16_t port_{9001};
  int listenFd_{-1};
  std::atomic<bool> running_{false};

  std::unordered_map<int, ClientState> clients_;
  std::unordered_map<std::string, std::unordered_map<std::string, int>> rooms_;
};

} // namespace signaling

#pragma once

#include "domain/room_store.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace signaling::domain {

class IConnection {
public:
  virtual ~IConnection() = default;
  virtual void sendText(std::string payload) = 0;
};

enum class RoomType {
  Direct,
  Group,
};

struct Participant {
  std::string peerId;
  std::string username;
};

struct JoinResult {
  bool ok{false};
  std::string error;
  std::string roomId;
  std::string peerId;
  RoomType roomType{RoomType::Direct};
  std::vector<Participant> participants;
  std::vector<std::shared_ptr<IConnection>> peersToNotify;
};

struct SignalRoute {
  std::shared_ptr<IConnection> target;
  std::string fromPeerId;
  std::string roomId;
};

struct LeaveResult {
  bool hadMembership{false};
  std::string roomId;
  std::string peerId;
  std::vector<std::shared_ptr<IConnection>> peersToNotify;
};

struct BroadcastResult {
    std::vector<std::shared_ptr<IConnection>> recipients;
    std::string fromPeerId;
    std::string roomId;
    int64_t messageSeq{0};
};

struct BufferedMessage {
    int64_t seq;
    std::string fromPeerId;
    std::string payload;
    std::string createdAt;
};

struct DisconnectResult {
    bool hadMembership{false};
    std::string roomId;
    std::string peerId;
    std::vector<std::shared_ptr<IConnection>> peersToNotify;
};

class RoomRegistry {
public:
  explicit RoomRegistry(std::shared_ptr<IRoomStore> store);

  JoinResult join(const std::shared_ptr<IConnection>& session,
                  std::string roomId,
                  std::string username,
                  std::optional<RoomType> requestedRoomType);

  std::optional<SignalRoute> route(const std::shared_ptr<IConnection>& sender,
                                   const std::string& roomId,
                                   const std::string& toPeerId,
                                   std::string& error);

    std::optional<BroadcastResult> broadcast(const std::shared_ptr<IConnection>& sender,
                                             const std::string& roomId,
                                             const std::string& payload,
                                             std::string& error);

  std::vector<BufferedMessage> getPendingMessages(const std::string& memberId);

  bool acknowledgeMessages(const std::shared_ptr<IConnection>& session,
                           const std::string& roomId,
                           int64_t upToSeq,
                           std::string& error);

  LeaveResult leave(const std::shared_ptr<IConnection>& session,
                    const std::optional<std::string>& roomIdCheck,
                    const std::optional<std::string>& peerIdCheck,
                    std::string& error);

  std::vector<DisconnectResult> disconnect(const std::shared_ptr<IConnection>& session);

private:
  struct ClientInfo {
    std::string roomId;
    std::string peerId;
    std::string username;
    RoomType roomType{RoomType::Direct};
  };

  static RoomType parseRoomType(const std::string& type);
  std::string generatePeerId();
  std::string currentTimestamp() const;
  std::string roomTypeToString(RoomType type) const;
  std::vector<std::shared_ptr<IConnection>> collectOnlinePeers(
      const std::string& roomId, const std::string& excludePeerId);

  // Find ClientInfo for a session in a specific room (nullptr if not found)
  ClientInfo* findClient(IConnection* session, const std::string& roomId);

  std::shared_ptr<IRoomStore> store_;
  std::mutex mu_;
  // IConnection* → client info per room (one entry per room joined)
  std::unordered_multimap<IConnection*, ClientInfo> clients_;
  // peerId → live session (for routing messages to peers)
  std::unordered_map<std::string, std::weak_ptr<IConnection>> connections_;
  std::uint64_t nextPeerSeq_{1};
};

} // namespace signaling::domain

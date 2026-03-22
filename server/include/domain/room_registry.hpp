#pragma once

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

class RoomRegistry {
public:
  JoinResult join(const std::shared_ptr<IConnection>& session,
                  std::string roomId,
                  std::string username,
                  std::optional<RoomType> requestedRoomType);

  std::optional<SignalRoute> route(const std::shared_ptr<IConnection>& sender,
                                   const std::string& roomId,
                                   const std::string& toPeerId,
                                   std::string& error);

  LeaveResult leave(const std::shared_ptr<IConnection>& session,
                    const std::optional<std::string>& roomIdCheck,
                    const std::optional<std::string>& peerIdCheck,
                    std::string& error);

private:
  struct ClientInfo {
    std::string roomId;
    std::string peerId;
    std::string username;
    RoomType roomType{RoomType::Direct};
  };

  struct MemberInfo {
    std::weak_ptr<IConnection> session;
    std::string username;
  };

  struct RoomInfo {
    RoomType roomType{RoomType::Direct};
    bool initialized{false};
    std::unordered_map<std::string, MemberInfo> members;
  };

  std::string generatePeerIdLocked(RoomInfo& room);

  std::mutex mu_;
  std::unordered_map<IConnection*, ClientInfo> clients_;
  std::unordered_map<std::string, RoomInfo> rooms_;
  std::uint64_t nextPeerSeq_{1};
};

} // namespace signaling::domain

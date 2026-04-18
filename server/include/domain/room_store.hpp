#pragma once

#include "domain/models.hpp"

#include <optional>
#include <string>
#include <vector>

namespace signaling::domain {

class IRoomStore {
public:
    virtual ~IRoomStore() = default;

    // Room CRUD
    virtual void saveRoom(const RoomRecord& room) = 0;
    virtual std::optional<RoomRecord> findRoom(const std::string& roomId) = 0;
    virtual void removeRoom(const std::string& roomId) = 0;

    // Member CRUD
    virtual void saveMember(const RoomMemberRecord& member) = 0;
    virtual std::optional<RoomMemberRecord> findMember(const std::string& memberId) = 0;
    virtual std::vector<RoomMemberRecord> findMembersByRoom(const std::string& roomId) = 0;
    virtual void removeMember(const std::string& memberId) = 0;

    // PeerSession CRUD
    virtual void saveSession(const PeerSessionRecord& session) = 0;
    virtual void removeSession(const std::string& peerId) = 0;
    virtual void removeSessionsByRoom(const std::string& roomId) = 0;
    virtual void clearAllSessions() = 0;
};

} // namespace signaling::domain

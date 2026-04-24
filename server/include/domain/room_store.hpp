#pragma once

#include "domain/models.hpp"

#include <cstdint>
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

    // Group message buffering
    virtual int64_t saveGroupMessage(const GroupMessageRecord& msg) = 0;
    virtual std::vector<GroupMessageRecord> getMessagesAfter(
        const std::string& roomId, int64_t afterSeq) = 0;
    virtual void updateLastAckedSeq(const std::string& memberId, int64_t seq) = 0;
    virtual void cleanupMessages(const std::string& roomId) = 0;
    virtual void removeGroupMessagesByRoom(const std::string& roomId) = 0;
};

} // namespace signaling::domain

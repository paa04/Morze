#pragma once

#include "domain/room_store.hpp"

#include <memory>
#include <string>

namespace signaling::infrastructure {

class SqliteRoomStore : public domain::IRoomStore {
public:
    explicit SqliteRoomStore(const std::string& dbPath);
    ~SqliteRoomStore() override;

    // Room CRUD
    void saveRoom(const domain::RoomRecord& room) override;
    std::optional<domain::RoomRecord> findRoom(const std::string& roomId) override;
    void removeRoom(const std::string& roomId) override;

    // Member CRUD
    void saveMember(const domain::RoomMemberRecord& member) override;
    std::optional<domain::RoomMemberRecord> findMember(const std::string& memberId) override;
    std::vector<domain::RoomMemberRecord> findMembersByRoom(const std::string& roomId) override;
    void removeMember(const std::string& memberId) override;

    // Group message buffering
    int64_t saveGroupMessage(const domain::GroupMessageRecord& msg) override;
    std::vector<domain::GroupMessageRecord> getMessagesAfter(
        const std::string& roomId, int64_t afterSeq) override;
    void updateLastAckedSeq(const std::string& memberId, int64_t seq) override;
    void cleanupMessages(const std::string& roomId) override;
    void removeGroupMessagesByRoom(const std::string& roomId) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace signaling::infrastructure

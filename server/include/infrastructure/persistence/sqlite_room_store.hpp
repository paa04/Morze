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

    // PeerSession CRUD
    void saveSession(const domain::PeerSessionRecord& session) override;
    void removeSession(const std::string& peerId) override;
    void removeSessionsByRoom(const std::string& roomId) override;
    void clearAllSessions() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace signaling::infrastructure

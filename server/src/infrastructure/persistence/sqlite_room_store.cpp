#include "infrastructure/persistence/sqlite_room_store.hpp"

#include <sqlite_orm/sqlite_orm.h>

namespace signaling::infrastructure {

namespace orm = sqlite_orm;

inline auto makeStorage(const std::string& dbPath) {
    using namespace domain;
    return orm::make_storage(
        dbPath,
        orm::make_table("rooms",
            orm::make_column("room_id", &RoomRecord::room_id),
            orm::make_column("type", &RoomRecord::type),
            orm::make_column("name", &RoomRecord::name),
            orm::make_column("name_suffix", &RoomRecord::name_suffix),
            orm::make_column("created_at", &RoomRecord::created_at),
            orm::primary_key(&RoomRecord::room_id)),
        orm::make_table("room_members",
            orm::make_column("member_id", &RoomMemberRecord::member_id),
            orm::make_column("username", &RoomMemberRecord::username),
            orm::make_column("last_online_at", &RoomMemberRecord::last_online_at),
            orm::make_column("room_id", &RoomMemberRecord::room_id),
            orm::primary_key(&RoomMemberRecord::member_id),
            orm::foreign_key(&RoomMemberRecord::room_id).references(&RoomRecord::room_id)),
        orm::make_table("peer_sessions",
            orm::make_column("peer_id", &PeerSessionRecord::peer_id),
            orm::make_column("connection_state", &PeerSessionRecord::connection_state),
            orm::make_column("member_id", &PeerSessionRecord::member_id),
            orm::primary_key(&PeerSessionRecord::peer_id),
            orm::foreign_key(&PeerSessionRecord::member_id).references(&RoomMemberRecord::member_id))
    );
}

using Storage = decltype(makeStorage(""));

struct SqliteRoomStore::Impl {
    Storage storage;

    explicit Impl(const std::string& dbPath)
        : storage(makeStorage(dbPath))
    {
        storage.sync_schema();
    }
};

SqliteRoomStore::SqliteRoomStore(const std::string& dbPath)
    : impl_(std::make_unique<Impl>(dbPath))
{}

SqliteRoomStore::~SqliteRoomStore() = default;

// --- Room ---

void SqliteRoomStore::saveRoom(const domain::RoomRecord& room) {
    impl_->storage.replace(room);
}

std::optional<domain::RoomRecord> SqliteRoomStore::findRoom(const std::string& roomId) {
    if (auto ptr = impl_->storage.get_pointer<domain::RoomRecord>(roomId)) {
        return *ptr;
    }
    return std::nullopt;
}

void SqliteRoomStore::removeRoom(const std::string& roomId) {
    // Remove dependent records first (foreign keys)
    auto members = findMembersByRoom(roomId);
    for (const auto& m : members) {
        removeSession(m.member_id); // remove sessions pointing to this member
        impl_->storage.remove<domain::RoomMemberRecord>(m.member_id);
    }
    impl_->storage.remove<domain::RoomRecord>(roomId);
}

// --- Member ---

void SqliteRoomStore::saveMember(const domain::RoomMemberRecord& member) {
    impl_->storage.replace(member);
}

std::optional<domain::RoomMemberRecord> SqliteRoomStore::findMember(const std::string& memberId) {
    if (auto ptr = impl_->storage.get_pointer<domain::RoomMemberRecord>(memberId)) {
        return *ptr;
    }
    return std::nullopt;
}

std::vector<domain::RoomMemberRecord> SqliteRoomStore::findMembersByRoom(const std::string& roomId) {
    using namespace domain;
    return impl_->storage.get_all<RoomMemberRecord>(
        orm::where(orm::c(&RoomMemberRecord::room_id) == roomId));
}

void SqliteRoomStore::removeMember(const std::string& memberId) {
    // Remove dependent peer sessions first
    using namespace domain;
    auto sessions = impl_->storage.get_all<PeerSessionRecord>(
        orm::where(orm::c(&PeerSessionRecord::member_id) == memberId));
    for (const auto& s : sessions) {
        impl_->storage.remove<PeerSessionRecord>(s.peer_id);
    }
    impl_->storage.remove<RoomMemberRecord>(memberId);
}

// --- PeerSession ---

void SqliteRoomStore::saveSession(const domain::PeerSessionRecord& session) {
    impl_->storage.replace(session);
}

void SqliteRoomStore::removeSession(const std::string& peerId) {
    impl_->storage.remove<domain::PeerSessionRecord>(peerId);
}

void SqliteRoomStore::removeSessionsByRoom(const std::string& roomId) {
    using namespace domain;
    auto members = findMembersByRoom(roomId);
    for (const auto& m : members) {
        auto sessions = impl_->storage.get_all<PeerSessionRecord>(
            orm::where(orm::c(&PeerSessionRecord::member_id) == m.member_id));
        for (const auto& s : sessions) {
            impl_->storage.remove<PeerSessionRecord>(s.peer_id);
        }
    }
}

void SqliteRoomStore::clearAllSessions() {
    impl_->storage.remove_all<domain::PeerSessionRecord>();
}

} // namespace signaling::infrastructure

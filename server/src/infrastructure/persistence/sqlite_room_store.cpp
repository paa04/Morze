#include "infrastructure/persistence/sqlite_room_store.hpp"

#include <sqlite_orm/sqlite_orm.h>

#include <algorithm>
#include <limits>

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
            orm::make_column("last_acked_msg_seq", &RoomMemberRecord::last_acked_msg_seq),
            orm::primary_key(&RoomMemberRecord::member_id),
            orm::foreign_key(&RoomMemberRecord::room_id).references(&RoomRecord::room_id)),
        orm::make_table("group_messages",
            orm::make_column("seq", &GroupMessageRecord::seq),
            orm::make_column("room_id", &GroupMessageRecord::room_id),
            orm::make_column("from_peer_id", &GroupMessageRecord::from_peer_id),
            orm::make_column("payload", &GroupMessageRecord::payload),
            orm::make_column("created_at", &GroupMessageRecord::created_at),
            orm::primary_key(&GroupMessageRecord::seq))
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
    auto members = findMembersByRoom(roomId);
    for (const auto& m : members) {
        impl_->storage.remove<domain::RoomMemberRecord>(m.member_id);
    }
    removeGroupMessagesByRoom(roomId);
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
    impl_->storage.remove<domain::RoomMemberRecord>(memberId);
}

// --- Group Messages ---

int64_t SqliteRoomStore::saveGroupMessage(const domain::GroupMessageRecord& msg) {
    return impl_->storage.insert(msg);
}

std::vector<domain::GroupMessageRecord> SqliteRoomStore::getMessagesAfter(
    const std::string& roomId, int64_t afterSeq) {
    using namespace domain;
    return impl_->storage.get_all<GroupMessageRecord>(
        orm::where(orm::c(&GroupMessageRecord::room_id) == roomId
                   && orm::c(&GroupMessageRecord::seq) > afterSeq),
        orm::order_by(&GroupMessageRecord::seq).asc());
}

void SqliteRoomStore::updateLastAckedSeq(const std::string& memberId, int64_t seq) {
    using namespace domain;
    auto member = impl_->storage.get_pointer<RoomMemberRecord>(memberId);
    if (member) {
        member->last_acked_msg_seq = seq;
        impl_->storage.update(*member);
    }
}

void SqliteRoomStore::cleanupMessages(const std::string& roomId) {
    using namespace domain;
    auto members = findMembersByRoom(roomId);
    if (members.empty()) {
        return;
    }

    int64_t minSeq = std::numeric_limits<int64_t>::max();
    for (const auto& m : members) {
        minSeq = std::min(minSeq, m.last_acked_msg_seq);
    }

    if (minSeq <= 0) {
        return;
    }

    auto toDelete = impl_->storage.get_all<GroupMessageRecord>(
        orm::where(orm::c(&GroupMessageRecord::room_id) == roomId
                   && orm::c(&GroupMessageRecord::seq) <= minSeq));
    for (const auto& msg : toDelete) {
        impl_->storage.remove<GroupMessageRecord>(msg.seq);
    }
}

void SqliteRoomStore::removeGroupMessagesByRoom(const std::string& roomId) {
    using namespace domain;
    auto msgs = impl_->storage.get_all<GroupMessageRecord>(
        orm::where(orm::c(&GroupMessageRecord::room_id) == roomId));
    for (const auto& msg : msgs) {
        impl_->storage.remove<GroupMessageRecord>(msg.seq);
    }
}

} // namespace signaling::infrastructure

#pragma once

#include <string>

namespace signaling::domain {

struct RoomRecord {
    std::string room_id;
    std::string type;       // "direct" / "group"
    std::string name;
    std::string name_suffix;
    std::string created_at; // ISO 8601
};

struct RoomMemberRecord {
    std::string member_id;
    std::string username;
    std::string last_online_at;
    std::string room_id;
    int64_t last_acked_msg_seq{0};
};

struct GroupMessageRecord {
    int64_t seq{0};           // auto-increment PK
    std::string room_id;
    std::string from_peer_id;
    std::string payload;      // JSON-serialized
    std::string created_at;
};

} // namespace signaling::domain

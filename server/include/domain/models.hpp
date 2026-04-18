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
};

struct PeerSessionRecord {
    std::string peer_id;
    std::string connection_state; // "connected" / "disconnected"
    std::string member_id;
};

} // namespace signaling::domain

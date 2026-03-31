#include "domain/room_registry.hpp"

namespace signaling::domain {

    std::string RoomRegistry::generatePeerIdLocked(RoomInfo &room) {
        while (true) {
            const std::string candidate = "peer" + std::to_string(nextPeerSeq_++);
            if (room.members.find(candidate) == room.members.end()) {
                return candidate;
            }
        }
    }

    JoinResult RoomRegistry::join(const std::shared_ptr<IConnection> &session,
                                  std::string roomId,
                                  std::string username,
                                  std::optional<RoomType> requestedRoomType) {
        std::lock_guard<std::mutex> lock(mu_);

        JoinResult out;
        if (roomId.empty() || username.empty()) {
            out.error = "roomId and username must be non-empty";
            return out;
        }

        if (clients_.find(session.get()) != clients_.end()) {
            out.error = "session already joined";
            return out;
        }

        auto &room = rooms_[roomId];
        for (auto it = room.members.begin(); it != room.members.end();) {
            if (it->second.session.expired()) {
                it = room.members.erase(it);
            } else {
                ++it;
            }
        }

        if (!room.initialized) {
            room.roomType = requestedRoomType.value_or(RoomType::Direct);
            room.initialized = true;
        } else if (requestedRoomType.has_value() && room.roomType != *requestedRoomType) {
            out.error = "room type mismatch";
            return out;
        }

        out.roomId = roomId;
        out.roomType = room.roomType;

        for (auto it = room.members.begin(); it != room.members.end();) {
            if (auto alive = it->second.session.lock()) {
                out.participants.push_back(Participant{it->first, it->second.username});
                out.peersToNotify.push_back(std::move(alive));
                ++it;
            } else {
                it = room.members.erase(it);
            }
        }

        if (room.roomType == RoomType::Direct && out.participants.size() >= 2) {
            out.error = "direct room supports at most 2 participants";
            return out;
        }

        out.peerId = generatePeerIdLocked(room);
        room.members.emplace(out.peerId, MemberInfo{session, username});

        clients_.emplace(
                session.get(),
                ClientInfo{roomId, out.peerId, std::move(username), room.roomType});

        out.ok = true;
        return out;
    }

    std::optional<SignalRoute> RoomRegistry::route(const std::shared_ptr<IConnection> &sender,
                                                   const std::string &roomId,
                                                   const std::string &toPeerId,
                                                   std::string &error) {
        std::lock_guard<std::mutex> lock(mu_);

        auto senderIt = clients_.find(sender.get());
        if (senderIt == clients_.end()) {
            error = "join room before signaling";
            return std::nullopt;
        }

        if (senderIt->second.roomId != roomId) {
            error = "roomId does not match active session room";
            return std::nullopt;
        }

        auto roomIt = rooms_.find(roomId);
        if (roomIt == rooms_.end()) {
            error = "room not found";
            return std::nullopt;
        }

        if (roomIt->second.roomType != RoomType::Direct) {
            error = "offer/answer/ice-candidate are allowed only in direct rooms";
            return std::nullopt;
        }

        auto targetIt = roomIt->second.members.find(toPeerId);
        if (targetIt == roomIt->second.members.end()) {
            error = "target peer not found";
            return std::nullopt;
        }

        auto target = targetIt->second.session.lock();
        if (!target) {
            roomIt->second.members.erase(targetIt);
            error = "target peer not found";
            return std::nullopt;
        }

        return SignalRoute{std::move(target), senderIt->second.peerId, roomId};
    }

    LeaveResult RoomRegistry::leave(const std::shared_ptr<IConnection> &session,
                                    const std::optional<std::string> &roomIdCheck,
                                    const std::optional<std::string> &peerIdCheck,
                                    std::string &error) {
        std::lock_guard<std::mutex> lock(mu_);

        LeaveResult out;

        auto clientIt = clients_.find(session.get());
        if (clientIt == clients_.end()) {
            if (roomIdCheck.has_value() || peerIdCheck.has_value()) {
                error = "session is not in any room";
            }
            return out;
        }

        if (roomIdCheck.has_value() && *roomIdCheck != clientIt->second.roomId) {
            error = "roomId does not match active session room";
            return out;
        }
        if (peerIdCheck.has_value() && *peerIdCheck != clientIt->second.peerId) {
            error = "peerId does not match active session peerId";
            return out;
        }

        out.hadMembership = true;
        out.roomId = clientIt->second.roomId;
        out.peerId = clientIt->second.peerId;

        auto roomIt = rooms_.find(out.roomId);
        if (roomIt != rooms_.end()) {
            roomIt->second.members.erase(out.peerId);

            for (auto it = roomIt->second.members.begin(); it != roomIt->second.members.end();) {
                if (auto alive = it->second.session.lock()) {
                    out.peersToNotify.push_back(std::move(alive));
                    ++it;
                } else {
                    it = roomIt->second.members.erase(it);
                }
            }

            if (roomIt->second.members.empty()) {
                rooms_.erase(roomIt);
            }
        }

        clients_.erase(clientIt);
        return out;
    }

} // namespace signaling::domain

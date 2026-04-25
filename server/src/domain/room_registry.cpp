#include "domain/room_registry.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>

namespace signaling::domain
{

    RoomRegistry::RoomRegistry(std::shared_ptr<IRoomStore> store)
        : store_(std::move(store))
    {}

    std::string RoomRegistry::currentTimestamp() const
    {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm buf{};
        gmtime_r(&time, &buf);
        std::ostringstream oss;
        oss << std::put_time(&buf, "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }

    std::string RoomRegistry::roomTypeToString(RoomType type) const
    {
        return type == RoomType::Direct ? "direct" : "group";
    }

    RoomType RoomRegistry::parseRoomType(const std::string &type)
    {
        return type == "group" ? RoomType::Group : RoomType::Direct;
    }

    std::string RoomRegistry::generatePeerId()
    {
        return "peer" + std::to_string(nextPeerSeq_++);
    }

    RoomRegistry::ClientInfo* RoomRegistry::findClient(IConnection* session, const std::string& roomId)
    {
        auto range = clients_.equal_range(session);
        for (auto it = range.first; it != range.second; ++it)
        {
            if (it->second.roomId == roomId)
            {
                return &it->second;
            }
        }
        return nullptr;
    }

    std::vector<std::shared_ptr<IConnection>> RoomRegistry::collectOnlinePeers(
        const std::string &roomId, const std::string &excludePeerId)
    {
        std::vector<std::shared_ptr<IConnection>> result;
        auto members = store_->findMembersByRoom(roomId);
        for (const auto &m : members)
        {
            if (m.member_id == excludePeerId)
            {
                continue;
            }
            auto connIt = connections_.find(m.member_id);
            if (connIt != connections_.end())
            {
                if (auto alive = connIt->second.lock())
                {
                    result.push_back(std::move(alive));
                } else
                {
                    connections_.erase(connIt);
                }
            }
        }
        return result;
    }

    JoinResult RoomRegistry::join(const std::shared_ptr<IConnection> &session,
                                  std::string roomId,
                                  std::string username,
                                  std::optional<RoomType> requestedRoomType)
    {
        std::lock_guard<std::mutex> lock(mu_);

        JoinResult out;
        if (roomId.empty() || username.empty())
        {
            out.error = "roomId and username must be non-empty";
            return out;
        }

        // Check if this session is already in THIS room
        if (findClient(session.get(), roomId) != nullptr)
        {
            out.error = "session already joined this room";
            return out;
        }

        auto now = currentTimestamp();

        // Check if room exists in DB
        auto roomOpt = store_->findRoom(roomId);
        RoomType roomType;

        if (roomOpt.has_value())
        {
            roomType = parseRoomType(roomOpt->type);
            if (requestedRoomType.has_value() && roomType != *requestedRoomType)
            {
                out.error = "room type mismatch";
                return out;
            }
        } else
        {
            // New room
            roomType = requestedRoomType.value_or(RoomType::Direct);
            store_->saveRoom(RoomRecord{
                roomId,
                roomTypeToString(roomType),
                roomId,
                "",
                now
            });
        }

        out.roomId = roomId;
        out.roomType = roomType;

        // Check for reconnection: existing member with same username
        auto members = store_->findMembersByRoom(roomId);
        std::string existingPeerId;
        int64_t existingAckedSeq{0};

        for (const auto &m : members)
        {
            if (m.username == username)
            {
                // Check if this member is online
                auto connIt = connections_.find(m.member_id);
                if (connIt != connections_.end() && connIt->second.lock())
                {
                    out.error = "username already in use in this room";
                    return out;
                }
                existingPeerId = m.member_id;
                existingAckedSeq = m.last_acked_msg_seq;
                break;
            }
        }

        bool isReconnect = !existingPeerId.empty();

        // Check Direct room capacity (only for new members)
        if (!isReconnect && roomType == RoomType::Direct && members.size() >= 2)
        {
            out.error = "direct room supports at most 2 participants";
            return out;
        }

        // Build participants list (online peers only, excluding self)
        for (const auto &m : members)
        {
            if (isReconnect && m.member_id == existingPeerId)
            {
                continue;
            }
            auto connIt = connections_.find(m.member_id);
            if (connIt != connections_.end())
            {
                if (auto alive = connIt->second.lock())
                {
                    out.participants.push_back(Participant{m.member_id, m.username});
                    out.peersToNotify.push_back(std::move(alive));
                } else
                {
                    connections_.erase(connIt);
                }
            }
        }

        if (isReconnect)
        {
            out.peerId = existingPeerId;
            // Update last_online_at, preserve ack cursor
            store_->saveMember(RoomMemberRecord{
                existingPeerId, username, now, roomId, existingAckedSeq
            });
        } else
        {
            out.peerId = generatePeerId();
            store_->saveMember(RoomMemberRecord{
                out.peerId, username, now, roomId
            });
        }

        // Register in RAM
        clients_.emplace(session.get(),
                         ClientInfo{roomId, out.peerId, username, roomType});
        connections_[out.peerId] = session;

        out.ok = true;
        return out;
    }

    std::optional<SignalRoute> RoomRegistry::route(const std::shared_ptr<IConnection> &sender,
                                                   const std::string &roomId,
                                                   const std::string &toPeerId,
                                                   std::string &error)
    {
        std::lock_guard<std::mutex> lock(mu_);

        auto *info = findClient(sender.get(), roomId);
        if (!info)
        {
            error = "join room before signaling";
            return std::nullopt;
        }

        if (info->roomType != RoomType::Direct)
        {
            error = "offer/answer/ice-candidate are allowed only in direct rooms";
            return std::nullopt;
        }

        auto connIt = connections_.find(toPeerId);
        if (connIt == connections_.end())
        {
            error = "target peer is offline";
            return std::nullopt;
        }

        auto target = connIt->second.lock();
        if (!target)
        {
            connections_.erase(connIt);
            error = "target peer is offline";
            return std::nullopt;
        }

        // Verify target peer is in the same room
        auto *targetInfo = findClient(target.get(), roomId);
        if (!targetInfo)
        {
            error = "target peer is not in this room";
            return std::nullopt;
        }

        return SignalRoute{std::move(target), info->peerId, roomId};
    }

    std::optional<BroadcastResult>
    RoomRegistry::broadcast(const std::shared_ptr<IConnection> &sender, const std::string &roomId,
                            const std::string &payload, std::string &error)
    {
        std::lock_guard<std::mutex> lock(mu_);

        auto *info = findClient(sender.get(), roomId);
        if (!info)
        {
            error = "join room before signaling";
            return std::nullopt;
        }

        if (info->roomType != RoomType::Group)
        {
            error = "group message allowed only in group chats";
            return std::nullopt;
        }

        // Save message to DB
        auto seq = store_->saveGroupMessage(GroupMessageRecord{
            0, roomId, info->peerId, payload, currentTimestamp()
        });

        auto recipients = collectOnlinePeers(roomId, info->peerId);
        return BroadcastResult{std::move(recipients), info->peerId, roomId, seq};
    }

    std::vector<BufferedMessage> RoomRegistry::getPendingMessages(const std::string &memberId)
    {
        std::lock_guard<std::mutex> lock(mu_);

        auto member = store_->findMember(memberId);
        if (!member.has_value())
        {
            return {};
        }

        auto msgs = store_->getMessagesAfter(member->room_id, member->last_acked_msg_seq);

        std::vector<BufferedMessage> result;
        result.reserve(msgs.size());
        for (const auto &m : msgs)
        {
            result.push_back(BufferedMessage{
                m.seq, m.from_peer_id, m.payload, m.created_at
            });
        }
        return result;
    }

    bool RoomRegistry::acknowledgeMessages(const std::shared_ptr<IConnection> &session,
                                           const std::string &roomId,
                                           int64_t upToSeq,
                                           std::string &error)
    {
        std::lock_guard<std::mutex> lock(mu_);

        auto *info = findClient(session.get(), roomId);
        if (!info)
        {
            error = "session is not in this room";
            return false;
        }

        store_->updateLastAckedSeq(info->peerId, upToSeq);
        store_->cleanupMessages(roomId);
        return true;
    }

    std::vector<DisconnectResult> RoomRegistry::disconnect(const std::shared_ptr<IConnection> &session)
    {
        std::lock_guard<std::mutex> lock(mu_);

        std::vector<DisconnectResult> results;

        auto range = clients_.equal_range(session.get());
        for (auto it = range.first; it != range.second; ++it)
        {
            DisconnectResult out;
            out.hadMembership = true;
            out.roomId = it->second.roomId;
            out.peerId = it->second.peerId;
            out.peersToNotify = collectOnlinePeers(out.roomId, out.peerId);
            connections_.erase(out.peerId);
            results.push_back(std::move(out));
        }

        clients_.erase(session.get());
        return results;
    }

    LeaveResult RoomRegistry::leave(const std::shared_ptr<IConnection> &session,
                                    const std::optional<std::string> &roomIdCheck,
                                    const std::optional<std::string> &peerIdCheck,
                                    std::string &error)
    {
        std::lock_guard<std::mutex> lock(mu_);

        LeaveResult out;

        // Find the specific entry for this room
        auto range = clients_.equal_range(session.get());
        auto clientIt = range.second; // will point to end of range if not found
        for (auto it = range.first; it != range.second; ++it)
        {
            bool roomMatch = !roomIdCheck.has_value() || *roomIdCheck == it->second.roomId;
            bool peerMatch = !peerIdCheck.has_value() || *peerIdCheck == it->second.peerId;
            if (roomMatch && peerMatch)
            {
                clientIt = it;
                break;
            }
        }

        if (clientIt == range.second)
        {
            if (range.first == range.second)
            {
                error = "session is not in any room";
            } else
            {
                error = "roomId/peerId does not match any active session room";
            }
            return out;
        }

        out.hadMembership = true;
        out.roomId = clientIt->second.roomId;
        out.peerId = clientIt->second.peerId;

        // Collect online peers to notify
        out.peersToNotify = collectOnlinePeers(out.roomId, out.peerId);

        // Remove from DB
        store_->removeMember(out.peerId);

        // Check if room is now empty → remove room
        auto remaining = store_->findMembersByRoom(out.roomId);
        if (remaining.empty())
        {
            store_->removeGroupMessagesByRoom(out.roomId);
            store_->removeRoom(out.roomId);
        } else
        {
            // Member left — their cursor no longer blocks cleanup
            store_->cleanupMessages(out.roomId);
        }

        // Remove from RAM
        connections_.erase(out.peerId);
        clients_.erase(clientIt);
        return out;
    }

} // namespace signaling::domain

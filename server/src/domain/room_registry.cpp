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
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
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

        if (clients_.find(session.get()) != clients_.end())
        {
            out.error = "session already joined";
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
            // Update last_online_at
            store_->saveMember(RoomMemberRecord{
                existingPeerId, username, now, roomId
            });
        } else
        {
            out.peerId = generatePeerId();
            store_->saveMember(RoomMemberRecord{
                out.peerId, username, now, roomId
            });
        }

        // Save peer session for group rooms
        if (roomType == RoomType::Group)
        {
            store_->saveSession(PeerSessionRecord{
                out.peerId, "connected", out.peerId
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

        auto senderIt = clients_.find(sender.get());
        if (senderIt == clients_.end())
        {
            error = "join room before signaling";
            return std::nullopt;
        }

        if (senderIt->second.roomId != roomId)
        {
            error = "roomId does not match active session room";
            return std::nullopt;
        }

        if (senderIt->second.roomType != RoomType::Direct)
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

        return SignalRoute{std::move(target), senderIt->second.peerId, roomId};
    }

    std::optional<BroadcastResult>
    RoomRegistry::broadcast(const std::shared_ptr<IConnection> &sender, const std::string &roomId,
                            const std::string &payload, std::string &error)
    {
        std::lock_guard<std::mutex> lock(mu_);

        auto senderIt = clients_.find(sender.get());

        if (senderIt == clients_.end())
        {
            error = "join room before signaling";
            return std::nullopt;
        }

        if (senderIt->second.roomId != roomId)
        {
            error = "roomId does not match active session room";
            return std::nullopt;
        }

        if (senderIt->second.roomType != RoomType::Group)
        {
            error = "group message allowed only in group chats";
            return std::nullopt;
        }

        // Save message to DB
        auto seq = store_->saveGroupMessage(GroupMessageRecord{
            0, roomId, senderIt->second.peerId, payload, currentTimestamp()
        });

        auto recipients = collectOnlinePeers(roomId, senderIt->second.peerId);
        return BroadcastResult{std::move(recipients), senderIt->second.peerId, roomId, seq};
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

        auto clientIt = clients_.find(session.get());
        if (clientIt == clients_.end())
        {
            error = "session is not in any room";
            return false;
        }

        if (clientIt->second.roomId != roomId)
        {
            error = "roomId does not match active session room";
            return false;
        }

        store_->updateLastAckedSeq(clientIt->second.peerId, upToSeq);
        store_->cleanupMessages(roomId);
        return true;
    }

    DisconnectResult RoomRegistry::disconnect(const std::shared_ptr<IConnection> &session)
    {
        std::lock_guard<std::mutex> lock(mu_);

        DisconnectResult out;

        auto clientIt = clients_.find(session.get());
        if (clientIt == clients_.end())
        {
            return out;
        }

        out.hadMembership = true;
        out.roomId = clientIt->second.roomId;
        out.peerId = clientIt->second.peerId;

        // Collect online peers to notify before removing connection
        out.peersToNotify = collectOnlinePeers(out.roomId, out.peerId);

        // Remove peer session from DB for group rooms
        if (clientIt->second.roomType == RoomType::Group)
        {
            store_->removeSession(out.peerId);
        }

        // Remove from RAM only — member stays in DB
        connections_.erase(out.peerId);
        clients_.erase(clientIt);
        return out;
    }

    LeaveResult RoomRegistry::leave(const std::shared_ptr<IConnection> &session,
                                    const std::optional<std::string> &roomIdCheck,
                                    const std::optional<std::string> &peerIdCheck,
                                    std::string &error)
    {
        std::lock_guard<std::mutex> lock(mu_);

        LeaveResult out;

        auto clientIt = clients_.find(session.get());
        if (clientIt == clients_.end())
        {
            if (roomIdCheck.has_value() || peerIdCheck.has_value())
            {
                error = "session is not in any room";
            }
            return out;
        }

        if (roomIdCheck.has_value() && *roomIdCheck != clientIt->second.roomId)
        {
            error = "roomId does not match active session room";
            return out;
        }
        if (peerIdCheck.has_value() && *peerIdCheck != clientIt->second.peerId)
        {
            error = "peerId does not match active session peerId";
            return out;
        }

        out.hadMembership = true;
        out.roomId = clientIt->second.roomId;
        out.peerId = clientIt->second.peerId;

        // Collect online peers to notify
        out.peersToNotify = collectOnlinePeers(out.roomId, out.peerId);

        // Remove from DB
        if (clientIt->second.roomType == RoomType::Group)
        {
            store_->removeSession(out.peerId);
        }
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

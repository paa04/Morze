#include "application/message_handler.hpp"

#include "application/protocol.hpp"

#include <boost/json.hpp>

namespace signaling::application
{

    namespace json = boost::json;

    namespace
    {

        std::string toText(const json::object &obj)
        {
            return json::serialize(obj);
        }

        std::optional<domain::RoomType> parseRoomType(std::string_view raw)
        {
            if (raw == "direct")
            {
                return domain::RoomType::Direct;
            }
            if (raw == "group")
            {
                return domain::RoomType::Group;
            }
            return std::nullopt;
        }

        std::string_view roomTypeToString(domain::RoomType type)
        {
            return type == domain::RoomType::Direct ? "direct" : "group";
        }

    } // namespace

    MessageHandler::MessageHandler(std::shared_ptr<domain::RoomRegistry> registry)
            : registry_(std::move(registry))
    {}

    void MessageHandler::handleMessage(const std::shared_ptr<domain::IConnection> &session,
                                       const std::string &raw)
    {
        std::string parseError;
        auto parsed = protocol::parseObject(raw, parseError);
        if (!parsed.has_value())
        {
            session->sendText(toText(protocol::makeError("invalid json: " + parseError)));
            return;
        }

        const auto type = protocol::getStringField(*parsed, "type");
        if (!type.has_value())
        {
            session->sendText(toText(protocol::makeError("field 'type' is required")));
            return;
        }


        if (*type == "join")
        {
            const auto roomId = protocol::getStringField(*parsed, "roomId");
            const auto username = protocol::getStringField(*parsed, "username");
            if (!roomId.has_value() || !username.has_value())
            {
                session->sendText(toText(protocol::makeError("join requires string fields: roomId, username")));
                return;
            }

            std::optional<domain::RoomType> requestedRoomType;
            if (auto roomTypeRaw = protocol::getStringField(*parsed, "roomType"); roomTypeRaw.has_value())
            {
                requestedRoomType = parseRoomType(*roomTypeRaw);
                if (!requestedRoomType.has_value())
                {
                    session->sendText(toText(protocol::makeError("roomType must be 'direct' or 'group'")));
                    return;
                }
            }

            auto result = registry_->join(session, *roomId, *username, requestedRoomType);
            if (!result.ok)
            {
                session->sendText(toText(protocol::makeError(result.error)));
                return;
            }

            session->sendText(toText(protocol::makeJoined(result.roomId,
                                                          roomTypeToString(result.roomType),
                                                          result.peerId,
                                                          result.participants)));

            // Deliver buffered messages for group rooms on reconnect
            if (result.roomType == domain::RoomType::Group)
            {
                auto pending = registry_->getPendingMessages(result.peerId);
                if (!pending.empty())
                {
                    session->sendText(toText(
                        protocol::makeBufferedMessages(result.roomId, pending)));
                }
            }

            auto joined = toText(protocol::makePeerJoined(result.roomId, result.peerId, *username));
            for (const auto &peer: result.peersToNotify)
            {
                peer->sendText(joined);
            }
            return;
        }

        if (*type == "offer" || *type == "answer" || *type == "ice-candidate")
        {
            const auto roomId = protocol::getStringField(*parsed, "roomId");
            const auto toPeerId = protocol::getStringField(*parsed, "toPeerId");
            const std::string payloadField = *type == "ice-candidate" ? "candidate" : "sdp";
            auto payloadIt = parsed->find(payloadField);

            if (!roomId.has_value() || !toPeerId.has_value() || payloadIt == parsed->end())
            {
                session->sendText(toText(protocol::makeError(
                        *type + " requires fields: roomId(string), toPeerId(string), " + payloadField + "(any)")));
                return;
            }

            std::string routeError;
            auto route = registry_->route(session, *roomId, *toPeerId, routeError);
            if (!route.has_value())
            {
                session->sendText(toText(protocol::makeError(routeError)));
                return;
            }

            if (*type == "offer")
            {
                route->target->sendText(toText(protocol::makeOffer(route->roomId,
                                                                   route->fromPeerId,
                                                                   *toPeerId,
                                                                   payloadIt->value())));
                return;
            }

            if (*type == "answer")
            {
                route->target->sendText(toText(protocol::makeAnswer(route->roomId,
                                                                    route->fromPeerId,
                                                                    *toPeerId,
                                                                    payloadIt->value())));
                return;
            }

            route->target->sendText(toText(protocol::makeIceCandidate(route->roomId,
                                                                      route->fromPeerId,
                                                                      *toPeerId,
                                                                      payloadIt->value())));
            return;
        }

        if (*type == "leave")
        {
            const auto roomId = protocol::getStringField(*parsed, "roomId");
            const auto peerId = protocol::getStringField(*parsed, "peerId");
            if (!roomId.has_value() || !peerId.has_value())
            {
                session->sendText(toText(protocol::makeError("leave requires fields: roomId, peerId")));
                return;
            }

            std::string leaveError;
            auto leave = registry_->leave(session, roomId, peerId, leaveError);
            if (!leaveError.empty())
            {
                session->sendText(toText(protocol::makeError(leaveError)));
                return;
            }

            if (leave.hadMembership)
            {
                auto leftMsg = toText(protocol::makePeerLeft(leave.roomId, leave.peerId));
                for (const auto &peer: leave.peersToNotify)
                {
                    peer->sendText(leftMsg);
                }
            }
            return;
        }

        if (*type == "group-message")
        {
            const auto roomId = protocol::getStringField(*parsed, "roomId");
            const auto payloadIt = parsed->find("payload");

            if (!roomId.has_value() || payloadIt == parsed->end())
            {
                session->sendText(toText(protocol::makeError("group-message requires fields: roomId, payload")));
                return;
            }

            std::string err;
            auto serializedPayload = json::serialize(payloadIt->value());

            auto broadcast_res = registry_->broadcast(session, roomId.value(), serializedPayload, err);

            if (!broadcast_res.has_value())
            {
                session->sendText(toText(protocol::makeError(err)));
                return;
            }

            auto message = protocol::makeGroupMessage(
                broadcast_res->roomId, broadcast_res->fromPeerId,
                broadcast_res->messageSeq, boost::json::value(payloadIt->value()));

            // Send to sender so they know the assigned messageSeq
            session->sendText(toText(message));

            for (const auto& member_conn : broadcast_res->recipients)
                member_conn->sendText(toText(message));

            return;
        }

        if (*type == "ack")
        {
            const auto roomId = protocol::getStringField(*parsed, "roomId");
            auto upToSeqIt = parsed->find("upToSeq");

            if (!roomId.has_value() || upToSeqIt == parsed->end() || !upToSeqIt->value().is_int64())
            {
                session->sendText(toText(protocol::makeError("ack requires fields: roomId, upToSeq(int)")));
                return;
            }

            std::string err;
            auto upToSeq = upToSeqIt->value().as_int64();
            bool ok = registry_->acknowledgeMessages(session, *roomId, upToSeq, err);
            if (!ok)
            {
                session->sendText(toText(protocol::makeError(err)));
                return;
            }

            session->sendText(toText(protocol::makeAckConfirm(*roomId, upToSeq)));
            return;
        }

        session->sendText(toText(protocol::makeError("unsupported message type")));
    }

    void MessageHandler::handleDisconnect(const std::shared_ptr<domain::IConnection> &session)
    {
        auto result = registry_->disconnect(session);
        if (!result.hadMembership)
        {
            return;
        }

        // Notify peers that this member went offline (not "left" — they're still a member)
        const auto leftMsg = toText(protocol::makePeerLeft(result.roomId, result.peerId));
        for (const auto &peer: result.peersToNotify)
        {
            peer->sendText(leftMsg);
        }
    }

} // namespace signaling::application

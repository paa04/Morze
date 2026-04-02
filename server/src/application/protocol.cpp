#include "application/protocol.hpp"

namespace signaling::application::protocol
{

    namespace json = boost::json;

    std::optional<json::object> parseObject(std::string_view raw, std::string &error)
    {
        boost::system::error_code ec;
        json::value value = json::parse(raw, ec);
        if (ec)
        {
            error = ec.message();
            return std::nullopt;
        }
        if (!value.is_object())
        {
            error = "json payload must be an object";
            return std::nullopt;
        }
        return value.as_object();
    }

    std::optional<std::string> getStringField(const json::object &obj, std::string_view key)
    {
        auto it = obj.find(key);
        if (it == obj.end() || !it->value().is_string())
        {
            return std::nullopt;
        }
        return std::string(it->value().as_string().c_str());
    }

    json::object makeError(std::string_view message)
    {
        json::object out;
        out["type"] = "error";
        out["message"] = message;
        return out;
    }

    json::object makeJoined(std::string_view roomId,
                            std::string_view roomType,
                            std::string_view peerId,
                            const std::vector<domain::Participant> &participants)
    {
        json::array arr;
        arr.reserve(participants.size());
        for (const auto &participant: participants)
        {
            json::object item;
            item["peerId"] = participant.peerId;
            item["username"] = participant.username;
            arr.emplace_back(std::move(item));
        }

        json::object out;
        out["type"] = "joined";
        out["roomId"] = roomId;
        out["roomType"] = roomType;
        out["peerId"] = peerId;
        out["participants"] = std::move(arr);
        return out;
    }

    json::object makePeerJoined(std::string_view roomId,
                                std::string_view peerId,
                                std::string_view username)
    {
        json::object out;
        out["type"] = "peer-joined";
        out["roomId"] = roomId;
        out["peerId"] = peerId;
        out["username"] = username;
        return out;
    }

    json::object makePeerLeft(std::string_view roomId, std::string_view peerId)
    {
        json::object out;
        out["type"] = "peer-left";
        out["roomId"] = roomId;
        out["peerId"] = peerId;
        return out;
    }

    json::object makeOffer(std::string_view roomId,
                           std::string_view fromPeerId,
                           std::string_view toPeerId,
                           const json::value &sdp)
    {
        json::object out;
        out["type"] = "offer";
        out["roomId"] = roomId;
        out["fromPeerId"] = fromPeerId;
        out["toPeerId"] = toPeerId;
        out["sdp"] = sdp;
        return out;
    }

    json::object makeAnswer(std::string_view roomId,
                            std::string_view fromPeerId,
                            std::string_view toPeerId,
                            const json::value &sdp)
    {
        json::object out;
        out["type"] = "answer";
        out["roomId"] = roomId;
        out["fromPeerId"] = fromPeerId;
        out["toPeerId"] = toPeerId;
        out["sdp"] = sdp;
        return out;
    }

    json::object makeIceCandidate(std::string_view roomId,
                                  std::string_view fromPeerId,
                                  std::string_view toPeerId,
                                  const json::value &candidate)
    {
        json::object out;
        out["type"] = "ice-candidate";
        out["roomId"] = roomId;
        out["fromPeerId"] = fromPeerId;
        out["toPeerId"] = toPeerId;
        out["candidate"] = candidate;
        return out;
    }

    json::object
    makeGroupMessage(std::string_view roomId, std::string_view fromPeerId, const boost::json::value& payload)
    {
        json::object out;
        out["type"] = "group-message";
        out["roomId"] = roomId;
        out["fromPeerId"] = fromPeerId;
        out["payload"] = payload;

        return out;
    }

} // namespace signaling::application::protocol

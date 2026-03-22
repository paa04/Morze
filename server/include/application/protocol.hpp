#pragma once

#include "domain/room_registry.hpp"

#include <boost/json.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace signaling::application::protocol {

std::optional<boost::json::object> parseObject(std::string_view raw, std::string& error);
std::optional<std::string> getStringField(const boost::json::object& obj, std::string_view key);

boost::json::object makeError(std::string_view message);
boost::json::object makeJoined(std::string_view roomId,
                               std::string_view roomType,
                               std::string_view peerId,
                               const std::vector<domain::Participant>& participants);
boost::json::object makePeerJoined(std::string_view roomId,
                                   std::string_view peerId,
                                   std::string_view username);
boost::json::object makePeerLeft(std::string_view roomId, std::string_view peerId);
boost::json::object makeOffer(std::string_view roomId,
                              std::string_view fromPeerId,
                              std::string_view toPeerId,
                              const boost::json::value& sdp);
boost::json::object makeAnswer(std::string_view roomId,
                               std::string_view fromPeerId,
                               std::string_view toPeerId,
                               const boost::json::value& sdp);
boost::json::object makeIceCandidate(std::string_view roomId,
                                     std::string_view fromPeerId,
                                     std::string_view toPeerId,
                                     const boost::json::value& candidate);

} // namespace signaling::application::protocol

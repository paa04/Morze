#include "signaling_server.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

namespace signaling {

namespace {
constexpr const char* kWebSocketGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

void appendUint64BigEndian(std::string& out, uint64_t value) {
  for (int i = 7; i >= 0; --i) {
    out.push_back(static_cast<char>((value >> (i * 8)) & 0xFF));
  }
}

std::string trim(std::string value) {
  auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
  value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
  value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
  return value;
}

bool sendAll(int fd, const char* data, size_t size) {
  size_t sent = 0;
  while (sent < size) {
    const auto n = ::send(fd, data + sent, size - sent, 0);
    if (n <= 0) {
      return false;
    }
    sent += static_cast<size_t>(n);
  }
  return true;
}

std::string sha1Binary(const std::string& input) {
  uint32_t h0 = 0x67452301;
  uint32_t h1 = 0xEFCDAB89;
  uint32_t h2 = 0x98BADCFE;
  uint32_t h3 = 0x10325476;
  uint32_t h4 = 0xC3D2E1F0;

  std::string msg = input;
  const uint64_t bitLen = static_cast<uint64_t>(msg.size()) * 8;

  msg.push_back(static_cast<char>(0x80));
  while ((msg.size() % 64) != 56) {
    msg.push_back(static_cast<char>(0x00));
  }

  for (int i = 7; i >= 0; --i) {
    msg.push_back(static_cast<char>((bitLen >> (i * 8)) & 0xFF));
  }

  for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
    uint32_t w[80]{};

    for (int i = 0; i < 16; ++i) {
      const size_t idx = chunk + static_cast<size_t>(i) * 4;
      w[i] = (static_cast<uint8_t>(msg[idx]) << 24) |
             (static_cast<uint8_t>(msg[idx + 1]) << 16) |
             (static_cast<uint8_t>(msg[idx + 2]) << 8) |
             static_cast<uint8_t>(msg[idx + 3]);
    }

    for (int i = 16; i < 80; ++i) {
      const uint32_t x = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
      w[i] = (x << 1) | (x >> 31);
    }

    uint32_t a = h0;
    uint32_t b = h1;
    uint32_t c = h2;
    uint32_t d = h3;
    uint32_t e = h4;

    for (int i = 0; i < 80; ++i) {
      uint32_t f = 0;
      uint32_t k = 0;

      if (i < 20) {
        f = (b & c) | ((~b) & d);
        k = 0x5A827999;
      } else if (i < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1;
      } else if (i < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDC;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6;
      }

      const uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
      e = d;
      d = c;
      c = (b << 30) | (b >> 2);
      b = a;
      a = temp;
    }

    h0 += a;
    h1 += b;
    h2 += c;
    h3 += d;
    h4 += e;
  }

  std::string out;
  out.reserve(20);

  auto pushWord = [&out](uint32_t w) {
    out.push_back(static_cast<char>((w >> 24) & 0xFF));
    out.push_back(static_cast<char>((w >> 16) & 0xFF));
    out.push_back(static_cast<char>((w >> 8) & 0xFF));
    out.push_back(static_cast<char>(w & 0xFF));
  };

  pushWord(h0);
  pushWord(h1);
  pushWord(h2);
  pushWord(h3);
  pushWord(h4);

  return out;
}

std::string base64EncodeBinary(const std::string& input) {
  static constexpr char kTable[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789+/";

  std::string out;
  out.reserve(((input.size() + 2) / 3) * 4);

  size_t i = 0;
  while (i + 3 <= input.size()) {
    const uint32_t n = (static_cast<uint8_t>(input[i]) << 16) |
                       (static_cast<uint8_t>(input[i + 1]) << 8) |
                       static_cast<uint8_t>(input[i + 2]);

    out.push_back(kTable[(n >> 18) & 0x3F]);
    out.push_back(kTable[(n >> 12) & 0x3F]);
    out.push_back(kTable[(n >> 6) & 0x3F]);
    out.push_back(kTable[n & 0x3F]);
    i += 3;
  }

  const size_t rem = input.size() - i;
  if (rem == 1) {
    const uint32_t n = static_cast<uint8_t>(input[i]) << 16;
    out.push_back(kTable[(n >> 18) & 0x3F]);
    out.push_back(kTable[(n >> 12) & 0x3F]);
    out.push_back('=');
    out.push_back('=');
  } else if (rem == 2) {
    const uint32_t n = (static_cast<uint8_t>(input[i]) << 16) |
                       (static_cast<uint8_t>(input[i + 1]) << 8);
    out.push_back(kTable[(n >> 18) & 0x3F]);
    out.push_back(kTable[(n >> 12) & 0x3F]);
    out.push_back(kTable[(n >> 6) & 0x3F]);
    out.push_back('=');
  }

  return out;
}
} // namespace

namespace protocol {

std::optional<std::string> getJsonStringField(const std::string& json, const std::string& key) {
  const auto raw = getJsonRawValue(json, key);
  if (!raw.has_value() || raw->size() < 2 || raw->front() != '"' || raw->back() != '"') {
    return std::nullopt;
  }

  std::string out;
  out.reserve(raw->size() - 2);

  bool escape = false;
  for (size_t i = 1; i + 1 < raw->size(); ++i) {
    const char c = (*raw)[i];
    if (escape) {
      switch (c) {
        case '"': out.push_back('"'); break;
        case '\\': out.push_back('\\'); break;
        case '/': out.push_back('/'); break;
        case 'b': out.push_back('\b'); break;
        case 'f': out.push_back('\f'); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        case 't': out.push_back('\t'); break;
        default: return std::nullopt;
      }
      escape = false;
      continue;
    }

    if (c == '\\') {
      escape = true;
      continue;
    }

    out.push_back(c);
  }

  if (escape) {
    return std::nullopt;
  }

  return out;
}

std::optional<std::string> getJsonRawValue(const std::string& json, const std::string& key) {
  const std::string needle = "\"" + key + "\"";
  size_t pos = json.find(needle);
  if (pos == std::string::npos) {
    return std::nullopt;
  }

  pos = json.find(':', pos + needle.size());
  if (pos == std::string::npos) {
    return std::nullopt;
  }

  ++pos;
  while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
    ++pos;
  }

  if (pos >= json.size()) {
    return std::nullopt;
  }

  const char first = json[pos];
  if (first == '"') {
    bool escape = false;
    for (size_t i = pos + 1; i < json.size(); ++i) {
      const char c = json[i];
      if (escape) {
        escape = false;
        continue;
      }

      if (c == '\\') {
        escape = true;
        continue;
      }

      if (c == '"') {
        return json.substr(pos, i - pos + 1);
      }
    }
    return std::nullopt;
  }

  if (first == '{' || first == '[') {
    const char open = first;
    const char close = (first == '{') ? '}' : ']';
    int depth = 0;
    bool inString = false;
    bool escape = false;

    for (size_t i = pos; i < json.size(); ++i) {
      const char c = json[i];

      if (inString) {
        if (escape) {
          escape = false;
        } else if (c == '\\') {
          escape = true;
        } else if (c == '"') {
          inString = false;
        }
        continue;
      }

      if (c == '"') {
        inString = true;
        continue;
      }

      if (c == open) {
        ++depth;
      } else if (c == close) {
        --depth;
        if (depth == 0) {
          return json.substr(pos, i - pos + 1);
        }
      }
    }

    return std::nullopt;
  }

  size_t end = pos;
  while (end < json.size() && json[end] != ',' && json[end] != '}') {
    ++end;
  }

  return trim(json.substr(pos, end - pos));
}

std::string jsonEscape(const std::string& input) {
  std::string out;
  out.reserve(input.size());

  for (unsigned char c : input) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          const char* hex = "0123456789ABCDEF";
          out += "\\u00";
          out.push_back(hex[(c >> 4) & 0x0F]);
          out.push_back(hex[c & 0x0F]);
        } else {
          out.push_back(static_cast<char>(c));
        }
    }
  }

  return out;
}

std::string wsAcceptValue(const std::string& wsKey) {
  return base64EncodeBinary(sha1Binary(wsKey + kWebSocketGuid));
}

bool decodeClientFrame(std::string& buffer,
                       std::string& payload,
                       uint8_t& opcode,
                       bool& isClose) {
  if (buffer.size() < 2) {
    opcode = 0;
    isClose = false;
    return true;
  }

  const uint8_t b0 = static_cast<uint8_t>(buffer[0]);
  const uint8_t b1 = static_cast<uint8_t>(buffer[1]);

  opcode = b0 & 0x0F;
  isClose = (opcode == 0x08);

  const bool masked = (b1 & 0x80) != 0;
  if (!masked) {
    return false;
  }

  uint64_t payloadLen = (b1 & 0x7F);
  size_t offset = 2;

  if (payloadLen == 126) {
    if (buffer.size() < offset + 2) {
      opcode = 0;
      isClose = false;
      return true;
    }

    payloadLen = (static_cast<uint8_t>(buffer[offset]) << 8) |
                 static_cast<uint8_t>(buffer[offset + 1]);
    offset += 2;
  } else if (payloadLen == 127) {
    if (buffer.size() < offset + 8) {
      opcode = 0;
      isClose = false;
      return true;
    }

    payloadLen = 0;
    for (int i = 0; i < 8; ++i) {
      payloadLen = (payloadLen << 8) | static_cast<uint8_t>(buffer[offset + i]);
    }
    offset += 8;
  }

  if (buffer.size() < offset + 4 + payloadLen) {
    opcode = 0;
    isClose = false;
    return true;
  }

  const std::string mask = buffer.substr(offset, 4);
  offset += 4;

  payload.resize(static_cast<size_t>(payloadLen));
  for (size_t i = 0; i < payload.size(); ++i) {
    payload[i] = buffer[offset + i] ^ mask[i % 4];
  }

  buffer.erase(0, offset + payload.size());
  return true;
}

} // namespace protocol

SignalingServer::SignalingServer(uint16_t port) : port_(port) {}

SignalingServer::~SignalingServer() {
  if (listenFd_ >= 0) {
    ::close(listenFd_);
  }

  for (const auto& [fd, client] : clients_) {
    (void)client;
    ::close(fd);
  }
}

bool SignalingServer::setupListenSocket() {
  listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listenFd_ < 0) {
    std::cerr << "socket() failed: " << strerror(errno) << '\n';
    return false;
  }

  int opt = 1;
  if (setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    std::cerr << "setsockopt() failed: " << strerror(errno) << '\n';
    return false;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port_);

  if (bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    std::cerr << "bind() failed: " << strerror(errno) << '\n';
    return false;
  }

  if (listen(listenFd_, 128) < 0) {
    std::cerr << "listen() failed: " << strerror(errno) << '\n';
    return false;
  }

  return true;
}

bool SignalingServer::start() {
  if (!setupListenSocket()) {
    return false;
  }

  running_.store(true);
  std::cout << "Signaling server started on ws://0.0.0.0:" << port_ << '\n';
  return true;
}

void SignalingServer::run() {
  while (running_.load()) {
    eventLoopTick();
  }
}

void SignalingServer::stop() {
  running_.store(false);
}

void SignalingServer::eventLoopTick() {
  fd_set readSet;
  FD_ZERO(&readSet);

  int maxFd = -1;
  if (listenFd_ >= 0) {
    FD_SET(listenFd_, &readSet);
    maxFd = listenFd_;
  }

  for (const auto& [fd, client] : clients_) {
    (void)client;
    FD_SET(fd, &readSet);
    maxFd = std::max(maxFd, fd);
  }

  timeval timeout{};
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;

  if (maxFd < 0) {
    return;
  }

  const int ready = select(maxFd + 1, &readSet, nullptr, nullptr, &timeout);
  if (ready < 0) {
    if (errno != EINTR) {
      std::cerr << "select() failed: " << strerror(errno) << '\n';
    }
    return;
  }

  if (listenFd_ >= 0 && FD_ISSET(listenFd_, &readSet)) {
    acceptClient();
  }

  std::vector<int> readableClients;
  readableClients.reserve(clients_.size());
  for (const auto& [fd, client] : clients_) {
    (void)client;
    if (FD_ISSET(fd, &readSet)) {
      readableClients.push_back(fd);
    }
  }

  for (int fd : readableClients) {
    if (clients_.contains(fd)) {
      handleClientReadable(fd);
    }
  }
}

void SignalingServer::acceptClient() {
  sockaddr_in clientAddr{};
  socklen_t addrLen = sizeof(clientAddr);

  int clientFd = accept(listenFd_, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
  if (clientFd < 0) {
    return;
  }

  ClientState state;
  state.fd = clientFd;
  clients_.emplace(clientFd, std::move(state));
}

void SignalingServer::handleClientReadable(int clientFd) {
  auto it = clients_.find(clientFd);
  if (it == clients_.end()) {
    return;
  }

  auto& client = it->second;
  std::array<char, 8192> buf{};
  const auto n = recv(clientFd, buf.data(), buf.size(), 0);
  if (n <= 0) {
    disconnectClient(clientFd);
    return;
  }

  client.readBuffer.append(buf.data(), static_cast<size_t>(n));

  if (!client.handshaken) {
    if (!processHandshake(client)) {
      if (client.handshaken) {
        return;
      }

      if (client.readBuffer.size() > 16384) {
        disconnectClient(clientFd);
      }
      return;
    }
  }

  if (client.handshaken) {
    if (!processFrames(client)) {
      disconnectClient(clientFd);
    }
  }
}

void SignalingServer::disconnectClient(int clientFd) {
  auto it = clients_.find(clientFd);
  if (it == clients_.end()) {
    return;
  }

  leaveRoom(it->second);
  ::close(clientFd);
  clients_.erase(it);
}

bool SignalingServer::processHandshake(ClientState& client) {
  const auto headerEnd = client.readBuffer.find("\r\n\r\n");
  if (headerEnd == std::string::npos) {
    return false;
  }

  std::string headers = client.readBuffer.substr(0, headerEnd + 4);
  client.readBuffer.erase(0, headerEnd + 4);

  std::istringstream stream(headers);
  std::string line;
  std::string wsKey;

  if (!std::getline(stream, line)) {
    return false;
  }

  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }

    const auto pos = line.find(':');
    if (pos == std::string::npos) {
      continue;
    }

    std::string key = line.substr(0, pos);
    std::string value = trim(line.substr(pos + 1));

    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });

    if (key == "sec-websocket-key") {
      wsKey = value;
    }
  }

  if (wsKey.empty()) {
    return false;
  }

  std::ostringstream response;
  response << "HTTP/1.1 101 Switching Protocols\r\n"
           << "Upgrade: websocket\r\n"
           << "Connection: Upgrade\r\n"
           << "Sec-WebSocket-Accept: " << wsAcceptValue(wsKey) << "\r\n"
           << "\r\n";

  const auto text = response.str();
  if (!sendAll(client.fd, text.data(), text.size())) {
    return false;
  }

  client.handshaken = true;
  return true;
}

bool SignalingServer::processFrames(ClientState& client) {
  while (true) {
    std::string payload;
    uint8_t opcode = 0;
    bool isClose = false;

    if (!decodeOneFrame(client.readBuffer, payload, opcode, isClose)) {
      return false;
    }

    if (opcode == 0x00 && payload.empty() && !isClose) {
      return true;
    }

    if (isClose || opcode == 0x08) {
      return false;
    }

    if (opcode == 0x09) {
      std::string pong;
      pong.push_back(static_cast<char>(0x8A));
      pong.push_back(static_cast<char>(payload.size()));
      pong += payload;
      if (!sendAll(client.fd, pong.data(), pong.size())) {
        return false;
      }
      continue;
    }

    if (opcode == 0x01) {
      handleMessage(client, payload);
      continue;
    }
  }
}

bool SignalingServer::decodeOneFrame(std::string& buffer,
                                     std::string& payload,
                                     uint8_t& opcode,
                                     bool& isClose) {
  return protocol::decodeClientFrame(buffer, payload, opcode, isClose);
}

bool SignalingServer::sendTextFrame(int fd, const std::string& text) {
  std::string frame;
  frame.push_back(static_cast<char>(0x81));

  const uint64_t size = text.size();
  if (size <= 125) {
    frame.push_back(static_cast<char>(size));
  } else if (size <= 65535) {
    frame.push_back(static_cast<char>(126));
    frame.push_back(static_cast<char>((size >> 8) & 0xFF));
    frame.push_back(static_cast<char>(size & 0xFF));
  } else {
    frame.push_back(static_cast<char>(127));
    appendUint64BigEndian(frame, size);
  }

  frame += text;
  return sendAll(fd, frame.data(), frame.size());
}

void SignalingServer::handleMessage(ClientState& client, const std::string& message) {
  const auto type = getJsonStringField(message, "type");
  if (!type.has_value()) {
    sendError(client.fd, "field 'type' is required");
    return;
  }

  if (*type == "join") {
    handleJoin(client, message);
    return;
  }

  if (*type == "signal") {
    handleSignal(client, message);
    return;
  }

  sendError(client.fd, "unsupported message type");
}

void SignalingServer::handleJoin(ClientState& client, const std::string& message) {
  const auto room = getJsonStringField(message, "room");
  const auto id = getJsonStringField(message, "id");

  if (!room.has_value() || !id.has_value() || room->empty() || id->empty()) {
    sendError(client.fd, "join requires string fields: room, id");
    return;
  }

  if (!client.roomId.empty()) {
    sendError(client.fd, "session already joined");
    return;
  }

  auto& members = rooms_[*room];
  if (members.contains(*id)) {
    sendError(client.fd, "peer id already exists in room");
    return;
  }

  std::vector<std::string> peers;
  peers.reserve(members.size());
  for (const auto& [peerId, fd] : members) {
    (void)fd;
    peers.push_back(peerId);
  }

  members[*id] = client.fd;
  client.roomId = *room;
  client.peerId = *id;

  sendPeers(client.fd, peers);
  broadcastPeerJoined(client.roomId, client.peerId, client.fd);
}

void SignalingServer::handleSignal(ClientState& client, const std::string& message) {
  if (client.roomId.empty() || client.peerId.empty()) {
    sendError(client.fd, "join room before signaling");
    return;
  }

  const auto to = getJsonStringField(message, "to");
  const auto data = getJsonRawValue(message, "data");
  if (!to.has_value() || !data.has_value()) {
    sendError(client.fd, "signal requires fields: to(string), data(any)");
    return;
  }

  auto roomIt = rooms_.find(client.roomId);
  if (roomIt == rooms_.end()) {
    sendError(client.fd, "room not found");
    return;
  }

  auto targetIt = roomIt->second.find(*to);
  if (targetIt == roomIt->second.end()) {
    sendError(client.fd, "target peer not found");
    return;
  }

  std::string payload = "{\"type\":\"signal\",\"from\":\"";
  payload += jsonEscape(client.peerId);
  payload += "\",\"data\":";
  payload += *data;
  payload += "}";

  if (!sendTextFrame(targetIt->second, payload)) {
    disconnectClient(targetIt->second);
  }
}

void SignalingServer::leaveRoom(ClientState& client) {
  if (client.roomId.empty() || client.peerId.empty()) {
    return;
  }

  std::string roomId = client.roomId;
  std::string peerId = client.peerId;

  auto roomIt = rooms_.find(roomId);
  if (roomIt != rooms_.end()) {
    roomIt->second.erase(peerId);
    if (roomIt->second.empty()) {
      rooms_.erase(roomIt);
    }
  }

  client.roomId.clear();
  client.peerId.clear();

  broadcastPeerLeft(roomId, peerId, client.fd);
}

void SignalingServer::sendError(int fd, const std::string& message) {
  std::string payload = "{\"type\":\"error\",\"message\":\"";
  payload += jsonEscape(message);
  payload += "\"}";
  if (!sendTextFrame(fd, payload)) {
    disconnectClient(fd);
  }
}

void SignalingServer::sendPeers(int fd, const std::vector<std::string>& peers) {
  std::string payload = "{\"type\":\"peers\",\"peers\":[";
  bool first = true;
  for (const auto& peer : peers) {
    if (!first) {
      payload += ',';
    }
    first = false;
    payload += '"';
    payload += jsonEscape(peer);
    payload += '"';
  }
  payload += "]}";

  if (!sendTextFrame(fd, payload)) {
    disconnectClient(fd);
  }
}

void SignalingServer::broadcastPeerJoined(const std::string& roomId,
                                          const std::string& peerId,
                                          int exceptFd) {
  auto roomIt = rooms_.find(roomId);
  if (roomIt == rooms_.end()) {
    return;
  }

  std::string payload = "{\"type\":\"peer-joined\",\"id\":\"";
  payload += jsonEscape(peerId);
  payload += "\"}";

  std::vector<int> broken;
  for (const auto& [_, fd] : roomIt->second) {
    if (fd == exceptFd) {
      continue;
    }
    if (!sendTextFrame(fd, payload)) {
      broken.push_back(fd);
    }
  }

  for (int fd : broken) {
    disconnectClient(fd);
  }
}

void SignalingServer::broadcastPeerLeft(const std::string& roomId,
                                        const std::string& peerId,
                                        int exceptFd) {
  auto roomIt = rooms_.find(roomId);
  if (roomIt == rooms_.end()) {
    return;
  }

  std::string payload = "{\"type\":\"peer-left\",\"id\":\"";
  payload += jsonEscape(peerId);
  payload += "\"}";

  std::vector<int> broken;
  for (const auto& [_, fd] : roomIt->second) {
    if (fd == exceptFd) {
      continue;
    }
    if (!sendTextFrame(fd, payload)) {
      broken.push_back(fd);
    }
  }

  for (int fd : broken) {
    disconnectClient(fd);
  }
}

std::optional<std::string> SignalingServer::getJsonStringField(const std::string& json,
                                                               const std::string& key) const {
  return protocol::getJsonStringField(json, key);
}

std::optional<std::string> SignalingServer::getJsonRawValue(const std::string& json,
                                                            const std::string& key) const {
  return protocol::getJsonRawValue(json, key);
}

std::string SignalingServer::jsonEscape(const std::string& input) {
  return protocol::jsonEscape(input);
}

std::string SignalingServer::wsAcceptValue(const std::string& wsKey) {
  return protocol::wsAcceptValue(wsKey);
}

std::string SignalingServer::sha1(const std::string& input) {
  return sha1Binary(input);
}

std::string SignalingServer::base64Encode(const std::string& input) {
  return base64EncodeBinary(input);
}

} // namespace signaling

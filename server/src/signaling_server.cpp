#include "signaling_server.hpp"

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/websocket.hpp>

#include <atomic>
#include <deque>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>

namespace signaling {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace json = boost::json;
using tcp = asio::ip::tcp;

namespace protocol {

std::optional<json::object> parseObject(std::string_view raw, std::string& error) {
  boost::system::error_code ec;
  json::value value = json::parse(raw, ec);
  if (ec) {
    error = ec.message();
    return std::nullopt;
  }
  if (!value.is_object()) {
    error = "json payload must be an object";
    return std::nullopt;
  }
  return value.as_object();
}

std::optional<std::string> getStringField(const json::object& obj, std::string_view key) {
  auto it = obj.find(key);
  if (it == obj.end() || !it->value().is_string()) {
    return std::nullopt;
  }
  return std::string(it->value().as_string().c_str());
}

json::object makeError(std::string_view message) {
  json::object out;
  out["type"] = "error";
  out["message"] = message;
  return out;
}

json::object makePeers(const std::vector<std::string>& peers) {
  json::array arr;
  arr.reserve(peers.size());
  for (const auto& peer : peers) {
    arr.emplace_back(peer);
  }

  json::object out;
  out["type"] = "peers";
  out["peers"] = std::move(arr);
  return out;
}

json::object makePeerJoined(std::string_view peerId) {
  json::object out;
  out["type"] = "peer-joined";
  out["id"] = peerId;
  return out;
}

json::object makePeerLeft(std::string_view peerId) {
  json::object out;
  out["type"] = "peer-left";
  out["id"] = peerId;
  return out;
}

json::object makeSignalForward(std::string_view fromPeerId, const json::value& data) {
  json::object out;
  out["type"] = "signal";
  out["from"] = fromPeerId;
  out["data"] = data;
  return out;
}

} // namespace protocol

class Session;

class RoomRegistry {
public:
  struct JoinResult {
    bool ok{false};
    std::string error;
    std::vector<std::string> existingPeers;
    std::vector<std::shared_ptr<Session>> peersToNotify;
  };

  struct SignalRoute {
    std::shared_ptr<Session> target;
    std::string fromPeerId;
  };

  struct LeaveResult {
    bool hadMembership{false};
    std::string peerId;
    std::vector<std::shared_ptr<Session>> peersToNotify;
  };

  JoinResult join(const std::shared_ptr<Session>& session, std::string roomId, std::string peerId);
  std::optional<SignalRoute> route(const std::shared_ptr<Session>& sender, const std::string& toPeerId);
  LeaveResult leave(const std::shared_ptr<Session>& session);

private:
  struct ClientInfo {
    std::string roomId;
    std::string peerId;
  };

  struct RoomInfo {
    std::unordered_map<std::string, std::weak_ptr<Session>> members;
  };

  std::mutex mu_;
  std::unordered_map<Session*, ClientInfo> clients_;
  std::unordered_map<std::string, RoomInfo> rooms_;
};

class Session : public std::enable_shared_from_this<Session> {
public:
  Session(tcp::socket socket, std::shared_ptr<RoomRegistry> registry)
      : ws_(std::move(socket)), registry_(std::move(registry)) {}

  void run() {
    ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
    ws_.set_option(websocket::stream_base::decorator([](websocket::response_type& res) {
      res.set(http::field::server, "MorzeSignaling/1.0");
    }));

    ws_.async_accept(
        beast::bind_front_handler(&Session::onAccept, shared_from_this()));
  }

  void sendJson(const json::object& payload) {
    const auto serialized = json::serialize(payload);
    auto self = shared_from_this();

    asio::post(ws_.get_executor(), [self, serialized]() mutable {
      const bool writing = !self->outbox_.empty();
      self->outbox_.push_back(std::move(serialized));
      if (!writing) {
        self->doWrite();
      }
    });
  }

private:
  void onAccept(beast::error_code ec) {
    if (ec) {
      return;
    }
    doRead();
  }

  void doRead() {
    ws_.async_read(buffer_, beast::bind_front_handler(&Session::onRead, shared_from_this()));
  }

  void onRead(beast::error_code ec, std::size_t) {
    if (ec == websocket::error::closed) {
      cleanup();
      return;
    }
    if (ec) {
      cleanup();
      return;
    }

    const std::string raw = beast::buffers_to_string(buffer_.data());
    buffer_.consume(buffer_.size());

    handleMessage(raw);
    doRead();
  }

  void doWrite() {
    ws_.text(true);
    ws_.async_write(asio::buffer(outbox_.front()),
                    beast::bind_front_handler(&Session::onWrite, shared_from_this()));
  }

  void onWrite(beast::error_code ec, std::size_t) {
    if (ec) {
      cleanup();
      return;
    }

    outbox_.pop_front();
    if (!outbox_.empty()) {
      doWrite();
    }
  }

  void handleMessage(const std::string& raw) {
    std::string parseError;
    auto parsed = protocol::parseObject(raw, parseError);
    if (!parsed.has_value()) {
      sendJson(protocol::makeError("invalid json: " + parseError));
      return;
    }

    const auto type = protocol::getStringField(*parsed, "type");
    if (!type.has_value()) {
      sendJson(protocol::makeError("field 'type' is required"));
      return;
    }

    if (*type == "join") {
      handleJoin(*parsed);
      return;
    }

    if (*type == "signal") {
      handleSignal(*parsed);
      return;
    }

    sendJson(protocol::makeError("unsupported message type"));
  }

  void handleJoin(const json::object& msg) {
    const auto room = protocol::getStringField(msg, "room");
    const auto id = protocol::getStringField(msg, "id");

    if (!room.has_value() || !id.has_value()) {
      sendJson(protocol::makeError("join requires string fields: room, id"));
      return;
    }

    auto result = registry_->join(shared_from_this(), *room, *id);
    if (!result.ok) {
      sendJson(protocol::makeError(result.error));
      return;
    }

    sendJson(protocol::makePeers(result.existingPeers));
    auto joined = protocol::makePeerJoined(*id);
    for (const auto& peer : result.peersToNotify) {
      peer->sendJson(joined);
    }
  }

  void handleSignal(const json::object& msg) {
    const auto to = protocol::getStringField(msg, "to");
    auto dataIt = msg.find("data");

    if (!to.has_value() || dataIt == msg.end()) {
      sendJson(protocol::makeError("signal requires fields: to(string), data(any)"));
      return;
    }

    auto route = registry_->route(shared_from_this(), *to);
    if (!route.has_value()) {
      sendJson(protocol::makeError("target peer not found"));
      return;
    }

    route->target->sendJson(protocol::makeSignalForward(route->fromPeerId, dataIt->value()));
  }

  void cleanup() {
    if (closed_) {
      return;
    }
    closed_ = true;

    auto leave = registry_->leave(shared_from_this());
    if (!leave.hadMembership) {
      return;
    }

    auto leftMsg = protocol::makePeerLeft(leave.peerId);
    for (const auto& peer : leave.peersToNotify) {
      peer->sendJson(leftMsg);
    }
  }

  websocket::stream<tcp::socket> ws_;
  std::shared_ptr<RoomRegistry> registry_;
  beast::flat_buffer buffer_;
  std::deque<std::string> outbox_;
  bool closed_{false};
};

RoomRegistry::JoinResult RoomRegistry::join(const std::shared_ptr<Session>& session,
                                            std::string roomId,
                                            std::string peerId) {
  std::lock_guard<std::mutex> lock(mu_);

  JoinResult out;
  if (roomId.empty() || peerId.empty()) {
    out.error = "room and id must be non-empty";
    return out;
  }

  if (clients_.contains(session.get())) {
    out.error = "session already joined";
    return out;
  }

  auto& room = rooms_[roomId];
  auto existing = room.members.find(peerId);
  if (existing != room.members.end()) {
    if (!existing->second.expired()) {
      out.error = "peer id already exists in room";
      return out;
    }
    room.members.erase(existing);
  }

  for (auto it = room.members.begin(); it != room.members.end();) {
    out.existingPeers.push_back(it->first);
    if (auto alive = it->second.lock()) {
      out.peersToNotify.push_back(std::move(alive));
      ++it;
    } else {
      it = room.members.erase(it);
    }
  }

  room.members.emplace(peerId, session);
  clients_.emplace(session.get(), ClientInfo{std::move(roomId), std::move(peerId)});
  out.ok = true;
  return out;
}

std::optional<RoomRegistry::SignalRoute> RoomRegistry::route(const std::shared_ptr<Session>& sender,
                                                             const std::string& toPeerId) {
  std::lock_guard<std::mutex> lock(mu_);

  auto senderIt = clients_.find(sender.get());
  if (senderIt == clients_.end()) {
    return std::nullopt;
  }

  auto roomIt = rooms_.find(senderIt->second.roomId);
  if (roomIt == rooms_.end()) {
    return std::nullopt;
  }

  auto targetIt = roomIt->second.members.find(toPeerId);
  if (targetIt == roomIt->second.members.end()) {
    return std::nullopt;
  }

  auto target = targetIt->second.lock();
  if (!target) {
    roomIt->second.members.erase(targetIt);
    return std::nullopt;
  }

  return SignalRoute{std::move(target), senderIt->second.peerId};
}

RoomRegistry::LeaveResult RoomRegistry::leave(const std::shared_ptr<Session>& session) {
  std::lock_guard<std::mutex> lock(mu_);

  LeaveResult out;

  auto clientIt = clients_.find(session.get());
  if (clientIt == clients_.end()) {
    return out;
  }

  out.hadMembership = true;
  out.peerId = clientIt->second.peerId;
  const std::string roomId = clientIt->second.roomId;

  auto roomIt = rooms_.find(roomId);
  if (roomIt != rooms_.end()) {
    roomIt->second.members.erase(clientIt->second.peerId);

    for (auto it = roomIt->second.members.begin(); it != roomIt->second.members.end();) {
      if (auto alive = it->second.lock()) {
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

class Listener : public std::enable_shared_from_this<Listener> {
public:
  Listener(asio::io_context& ioc, tcp::endpoint endpoint, std::shared_ptr<RoomRegistry> registry)
      : ioc_(ioc), acceptor_(ioc), registry_(std::move(registry)) {
    beast::error_code ec;

    acceptor_.open(endpoint.protocol(), ec);
    if (ec) {
      throw std::runtime_error("failed to open acceptor: " + ec.message());
    }

    acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
    if (ec) {
      throw std::runtime_error("failed to set reuse_address: " + ec.message());
    }

    acceptor_.bind(endpoint, ec);
    if (ec) {
      throw std::runtime_error("failed to bind: " + ec.message());
    }

    acceptor_.listen(asio::socket_base::max_listen_connections, ec);
    if (ec) {
      throw std::runtime_error("failed to listen: " + ec.message());
    }
  }

  void run() {
    doAccept();
  }

private:
  void doAccept() {
    acceptor_.async_accept(
        asio::make_strand(ioc_),
        beast::bind_front_handler(&Listener::onAccept, shared_from_this()));
  }

  void onAccept(beast::error_code ec, tcp::socket socket) {
    if (!ec) {
      std::make_shared<Session>(std::move(socket), registry_)->run();
    }

    doAccept();
  }

  asio::io_context& ioc_;
  tcp::acceptor acceptor_;
  std::shared_ptr<RoomRegistry> registry_;
};

struct SignalingServer::Impl {
  explicit Impl(uint16_t p, std::size_t t)
      : port(p), threads(std::max<std::size_t>(1, t)), ioc(static_cast<int>(threads)) {}

  uint16_t port;
  std::size_t threads;
  asio::io_context ioc;
  std::shared_ptr<RoomRegistry> registry = std::make_shared<RoomRegistry>();
  std::shared_ptr<Listener> listener;
  std::vector<std::thread> workers;
  std::atomic<bool> started{false};
};

SignalingServer::SignalingServer(uint16_t port, std::size_t threads)
    : impl_(std::make_unique<Impl>(port, threads)) {}

SignalingServer::~SignalingServer() {
  stop();
}

bool SignalingServer::start() {
  if (impl_->started.exchange(true)) {
    return false;
  }

  try {
    impl_->listener = std::make_shared<Listener>(
        impl_->ioc,
        tcp::endpoint{tcp::v4(), impl_->port},
        impl_->registry);
    impl_->listener->run();
  } catch (const std::exception& ex) {
    std::cerr << "Failed to start signaling server: " << ex.what() << '\n';
    impl_->started.store(false);
    return false;
  }

  std::cout << "Signaling server started on ws://0.0.0.0:" << impl_->port << '\n';
  return true;
}

void SignalingServer::run() {
  if (!impl_->started.load()) {
    return;
  }

  impl_->workers.reserve(impl_->threads);
  for (std::size_t i = 0; i < impl_->threads; ++i) {
    impl_->workers.emplace_back([this]() {
      impl_->ioc.run();
    });
  }

  for (auto& worker : impl_->workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }
  impl_->workers.clear();
}

void SignalingServer::stop() {
  if (!impl_->started.exchange(false)) {
    return;
  }

  impl_->ioc.stop();

  for (auto& worker : impl_->workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }
  impl_->workers.clear();
}

} // namespace signaling

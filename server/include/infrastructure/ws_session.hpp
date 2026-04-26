#pragma once

#include "application/message_handler.hpp"
#include "domain/room_registry.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/websocket.hpp>

#include <deque>
#include <memory>
#include <string>

namespace signaling::infrastructure {

class WsSession : public domain::IConnection,
                  public std::enable_shared_from_this<WsSession> {
public:
  WsSession(boost::asio::ip::tcp::socket socket,
            std::shared_ptr<application::MessageHandler> handler);

  void run();
  /// Accept with a pre-read HTTP upgrade request (used by HttpDetectSession)
  void run(boost::beast::http::request<boost::beast::http::string_body> req,
           boost::beast::flat_buffer buffer);
  void sendText(std::string payload) override;

private:
  void onAccept(boost::beast::error_code ec);
  void doRead();
  void onRead(boost::beast::error_code ec, std::size_t);
  void doWrite();
  void onWrite(boost::beast::error_code ec, std::size_t);
  void cleanup();

  boost::beast::websocket::stream<boost::asio::ip::tcp::socket> ws_;
  std::shared_ptr<application::MessageHandler> handler_;
  boost::beast::flat_buffer buffer_;
  std::deque<std::string> outbox_;
  bool closed_{false};
};

} // namespace signaling::infrastructure

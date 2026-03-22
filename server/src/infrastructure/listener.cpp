#include "infrastructure/listener.hpp"

#include "infrastructure/ws_session.hpp"

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>

#include <stdexcept>
#include <utility>

namespace signaling::infrastructure {

namespace asio = boost::asio;
namespace beast = boost::beast;
using tcp = asio::ip::tcp;

Listener::Listener(asio::io_context& ioc,
                   tcp::endpoint endpoint,
                   std::shared_ptr<application::MessageHandler> handler)
    : ioc_(ioc), acceptor_(ioc), handler_(std::move(handler)) {
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

void Listener::run() {
  doAccept();
}

void Listener::doAccept() {
  acceptor_.async_accept(
      asio::make_strand(ioc_),
      beast::bind_front_handler(&Listener::onAccept, shared_from_this()));
}

void Listener::onAccept(beast::error_code ec, tcp::socket socket) {
  if (!ec) {
    std::make_shared<WsSession>(std::move(socket), handler_)->run();
  }

  doAccept();
}

} // namespace signaling::infrastructure

#include "infrastructure/http_detect_session.hpp"
#include "infrastructure/ws_session.hpp"

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/json.hpp>

#include <random>

namespace signaling::infrastructure {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

HttpDetectSession::HttpDetectSession(tcp::socket socket,
                                     std::shared_ptr<application::MessageHandler> handler,
                                     int canaryPercent)
    : socket_(std::move(socket))
    , handler_(std::move(handler))
    , canaryPercent_(canaryPercent)
{}

void HttpDetectSession::run()
{
    http::async_read(socket_, buffer_, req_,
        beast::bind_front_handler(&HttpDetectSession::onRead, shared_from_this()));
}

void HttpDetectSession::onRead(beast::error_code ec, std::size_t)
{
    if (ec) return;

    // WebSocket upgrade → delegate to WsSession
    if (websocket::is_upgrade(req_)) {
        auto ws = std::make_shared<WsSession>(std::move(socket_), handler_);
        ws->run(std::move(req_), std::move(buffer_));
        return;
    }

    // GET /canary → return JSON flag
    if (req_.method() == http::verb::get && req_.target() == "/canary") {
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, 99);
        bool isCanary = dist(rng) < canaryPercent_;

        boost::json::object body;
        body["canary"] = isCanary;

        http::response<http::string_body> res{http::status::ok, req_.version()};
        res.set(http::field::server, "MorzeSignaling/1.0");
        res.set(http::field::content_type, "application/json");
        res.keep_alive(false);
        res.body() = boost::json::serialize(body);
        res.prepare_payload();

        auto sp = std::make_shared<http::response<http::string_body>>(std::move(res));
        http::async_write(socket_, *sp,
            [self = shared_from_this(), sp](beast::error_code ec, std::size_t) {
                self->socket_.shutdown(tcp::socket::shutdown_send, ec);
            });
        return;
    }

    // Any other HTTP request → 404
    http::response<http::string_body> res{http::status::not_found, req_.version()};
    res.set(http::field::server, "MorzeSignaling/1.0");
    res.set(http::field::content_type, "text/plain");
    res.keep_alive(false);
    res.body() = "Not Found";
    res.prepare_payload();

    auto sp = std::make_shared<http::response<http::string_body>>(std::move(res));
    http::async_write(socket_, *sp,
        [self = shared_from_this(), sp](beast::error_code ec, std::size_t) {
            self->socket_.shutdown(tcp::socket::shutdown_send, ec);
        });
}

} // namespace signaling::infrastructure

#include "infrastructure/ws_session.hpp"

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>

#include <utility>

namespace signaling::infrastructure {

    namespace asio = boost::asio;
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace websocket = beast::websocket;

    WsSession::WsSession(boost::asio::ip::tcp::socket socket,
                         std::shared_ptr<application::MessageHandler> handler)
            : ws_(std::move(socket)), handler_(std::move(handler)) {}

    void WsSession::run() {
        ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
        ws_.set_option(websocket::stream_base::decorator([](websocket::response_type &res) {
            res.set(http::field::server, "MorzeSignaling/1.0");
        }));

        ws_.async_accept(beast::bind_front_handler(&WsSession::onAccept, shared_from_this()));
    }

    void WsSession::sendText(std::string payload) {
        auto self = shared_from_this();
        asio::post(ws_.get_executor(), [self, payload = std::move(payload)]() mutable {
            if (self->closed_) {
                return;
            }
            const bool writing = !self->outbox_.empty();
            self->outbox_.push_back(std::move(payload));
            if (!writing) {
                self->doWrite();
            }
        });
    }

    void WsSession::onAccept(beast::error_code ec) {
        if (ec) {
            return;
        }
        doRead();
    }

    void WsSession::doRead() {
        ws_.async_read(buffer_, beast::bind_front_handler(&WsSession::onRead, shared_from_this()));
    }

    void WsSession::onRead(beast::error_code ec, std::size_t) {
        if (ec == websocket::error::closed || ec) {
            cleanup();
            return;
        }

        const std::string raw = beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());

        handler_->handleMessage(shared_from_this(), raw);
        doRead();
    }

    void WsSession::doWrite() {
        if (closed_) {
            return;
        }
        ws_.text(true);
        ws_.async_write(asio::buffer(outbox_.front()),
                        beast::bind_front_handler(&WsSession::onWrite, shared_from_this()));
    }

    void WsSession::onWrite(beast::error_code ec, std::size_t) {
        if (ec) {
            cleanup();
            return;
        }

        outbox_.pop_front();
        if (!outbox_.empty()) {
            doWrite();
        }
    }

    void WsSession::cleanup() {
        if (closed_) {
            return;
        }
        closed_ = true;
        outbox_.clear();
        handler_->handleDisconnect(shared_from_this());
    }

} // namespace signaling::infrastructure

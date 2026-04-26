#pragma once

#include "application/message_handler.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>

#include <memory>

namespace signaling::infrastructure {

class HttpDetectSession : public std::enable_shared_from_this<HttpDetectSession> {
public:
    HttpDetectSession(boost::asio::ip::tcp::socket socket,
                      std::shared_ptr<application::MessageHandler> handler,
                      int canaryPercent);

    void run();

private:
    void onRead(boost::beast::error_code ec, std::size_t);
    void onWrite(boost::beast::error_code ec, std::size_t, bool close);

    boost::asio::ip::tcp::socket socket_;
    std::shared_ptr<application::MessageHandler> handler_;
    int canaryPercent_;
    boost::beast::flat_buffer buffer_;
    boost::beast::http::request<boost::beast::http::string_body> req_;
};

} // namespace signaling::infrastructure

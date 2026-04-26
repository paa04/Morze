#pragma once

#include "application/message_handler.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/beast/core/error.hpp>

#include <memory>

namespace signaling::infrastructure {

    class Listener : public std::enable_shared_from_this<Listener> {
    public:
        Listener(boost::asio::io_context &ioc,
                 boost::asio::ip::tcp::endpoint endpoint,
                 std::shared_ptr<application::MessageHandler> handler,
                 int canaryPercent = 50);

        void run();

    private:
        void doAccept();

        void onAccept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket);

        boost::asio::io_context &ioc_;
        boost::asio::ip::tcp::acceptor acceptor_;
        std::shared_ptr<application::MessageHandler> handler_;
        int canaryPercent_;
    };

} // namespace signaling::infrastructure

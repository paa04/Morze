//
// Created by paa04 on 31.03.2026.
//

#ifndef MORZE_SIGNALING_SERVER_WS_TEST_CLIENT_HPP
#define MORZE_SIGNALING_SERVER_WS_TEST_CLIENT_HPP


#include <string_view>
#include <cstdint>
#include <boost/json/object.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/connect.hpp>
#include <boost/beast.hpp>
#include <string>



class ws_test_client
{
private:
    boost::asio::io_context ioc;
    boost::asio::ip::tcp::resolver resolver{ioc};
    boost::beast::websocket::stream<boost::beast::tcp_stream> ws{ioc};

public:


    bool connect(std::string_view host, uint16_t port);

    bool sendJson(const boost::json::object &);

    boost::json::value recvJson();

    bool close();
};


#endif //MORZE_SIGNALING_SERVER_WS_TEST_CLIENT_HPP

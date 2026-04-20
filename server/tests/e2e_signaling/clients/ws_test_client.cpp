//
// Created by paa04 on 31.03.2026.
//


#include <boost/json/serialize.hpp>
#include <boost/json/parse.hpp>
#include "ws_test_client.hpp"


bool ws_test_client::connect(std::string_view host, uint16_t port)
{
    auto results = resolver.resolve(std::string(host), std::to_string(port));

    ws.next_layer().expires_after(std::chrono::seconds(3));
    ws.next_layer().connect(results);

    ws.next_layer().expires_never();
    ws.set_option(boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::client));

    const std::string host_str(host);
    ws.handshake(host_str + ":" + std::to_string(port), "/");

    return true;
}

bool ws_test_client::sendJson(const boost::json::object &msg)
{
    const auto payload = boost::json::serialize(msg);
    const auto buff = boost::asio::buffer(payload);

    ws.text(true);
    ws.write(buff);
    return true;
}

boost::json::value ws_test_client::recvJson()
{
    boost::beast::flat_buffer read_buff;

    ws.next_layer().expires_after(std::chrono::seconds(3));

    ws.read(read_buff);

    ws.next_layer().expires_never();

    std::string msg_text = boost::beast::buffers_to_string(read_buff.data());
    boost::json::value value = boost::json::parse(msg_text);

    return value;
}

bool ws_test_client::close()
{
    ws.close(boost::beast::websocket::close_code::normal);
    return true;
}
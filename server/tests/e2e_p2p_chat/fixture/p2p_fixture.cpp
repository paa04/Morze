//
// Created by paa04 on 01.04.2026.
//
#include <boost/asio.hpp>
#include "p2p_fixture.hpp"

std::atomic<uint16_t> TestFixtureP2p::next_port{19000};

bool readyCheck(uint16_t port, int timeout_ms = 3000)
{
    namespace asio = boost::asio;
    using tcp = asio::ip::tcp;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

    while (std::chrono::steady_clock::now() < deadline)
    {
        asio::io_context ioc;
        tcp::socket sock(ioc);
        boost::system::error_code ec;

        sock.connect({asio::ip::make_address("127.0.0.1"), port}, ec);

        if (!ec)
        {
            boost::system::error_code ignored;
            sock.close(ignored);

            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(25));

    }

    return false;
}

void TestFixtureP2p::SetUp()
{
    port = next_port.fetch_add(1);

    server_ptr = std::make_unique<signaling::SignalingServer>(port, threads);

    ASSERT_TRUE(server_ptr->start());

    server_thrd = std::thread([this]
                              { server_ptr->run(); });

    ASSERT_TRUE(readyCheck(port));

}

void TestFixtureP2p::TearDown()
{
    if (server_ptr)
        server_ptr->stop();

    if (server_thrd.joinable())
        server_thrd.join();
}
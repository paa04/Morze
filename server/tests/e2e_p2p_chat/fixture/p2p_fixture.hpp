#pragma once

#include <gtest/gtest.h>
#include "../../../include/signaling_server.hpp"

class TestFixtureP2p: public ::testing::Test
{
protected:

    uint16_t port;
    size_t threads = 1;

    static std::atomic<uint16_t> next_port;

    std::unique_ptr<signaling::SignalingServer> server_ptr;
    std::thread server_thrd;

    void SetUp() override;

    void TearDown() override;
};
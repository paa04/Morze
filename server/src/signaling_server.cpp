#include "signaling_server.hpp"

#include "application/message_handler.hpp"
#include "domain/room_registry.hpp"
#include "infrastructure/listener.hpp"
#include "infrastructure/persistence/sqlite_room_store.hpp"

#include <boost/asio.hpp>

#include <algorithm>
#include <atomic>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

namespace signaling
{

    namespace asio = boost::asio;
    using tcp = asio::ip::tcp;

    struct SignalingServer::Impl
    {
        using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

        explicit Impl(uint16_t p, std::size_t t, std::string db)
                : port(p), threads(std::max<std::size_t>(1, t)),
                  dbPath(std::move(db)),
                  ioc(static_cast<int>(threads)),
                  work_guard(boost::asio::make_work_guard(ioc))
        {
            if (dbPath.empty())
            {
                dbPath = ":memory:";
            }
            auto store = std::make_shared<infrastructure::SqliteRoomStore>(dbPath);
            store->clearAllSessions();
            registry = std::make_shared<domain::RoomRegistry>(store);
            handler = std::make_shared<application::MessageHandler>(registry);
        }

        uint16_t port;
        std::size_t threads;
        std::string dbPath;

        asio::io_context ioc;
        WorkGuard work_guard;

        std::shared_ptr<domain::RoomRegistry> registry;
        std::shared_ptr<application::MessageHandler> handler;
        std::shared_ptr<infrastructure::Listener> listener;
        std::vector<std::thread> workers;

        std::atomic<bool> started{false};
        std::atomic<bool> running{false};

    };

    SignalingServer::SignalingServer(uint16_t port, std::size_t threads, std::string dbPath)
            : impl_(std::make_unique<Impl>(port, threads, std::move(dbPath)))
    {}

    SignalingServer::~SignalingServer()
    {
        stop();
    }

    bool SignalingServer::start()
    {
        if (impl_->started.exchange(true))
        {
            return false;
        }

        try
        {
            impl_->listener = std::make_shared<infrastructure::Listener>(
                    impl_->ioc,
                    tcp::endpoint{tcp::v4(), impl_->port},
                    impl_->handler);
            impl_->listener->run();
        } catch (const std::exception &ex)
        {
            std::cerr << "Failed to start signaling server: " << ex.what() << '\n';
            impl_->started.store(false);
            return false;
        }

        std::cout << "Signaling server started on ws://0.0.0.0:" << impl_->port << '\n';
        return true;
    }

    void SignalingServer::run()
    {
        if (!impl_->started.load())
        {
            return;
        }

        if (impl_->running.exchange(true))
        {
            return;
        }

        impl_->workers.reserve(impl_->threads);
        for (std::size_t i = 0; i < impl_->threads; ++i)
        {
            impl_->workers.emplace_back([this]()
                                        {
                                            impl_->ioc.run();
                                        });
        }

        for (auto &worker: impl_->workers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
        impl_->workers.clear();
        impl_->running.store(false);
    }

    void SignalingServer::stop()
    {
        if (!impl_->started.exchange(false))
        {
            return;
        }

        impl_->work_guard.reset();
        impl_->ioc.stop();
    }

} // namespace signaling

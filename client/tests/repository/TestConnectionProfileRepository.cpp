#include <doctest/doctest.h>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <chrono>
#include <vector>
#include <algorithm>

#include "ConnectionProfileRepository.h"
#include "ConnectionProfileExceptions.h"
#include "ConnectionProfileModel.h"
#include "ConnectionProfileDAOConverter.h"
#include "ConnectionProfileDAO.h"
#include "DBConfiguration.h"
#include "TimePointConverter.h"

namespace asio = boost::asio;

template<typename Awaitable>
auto run_async(asio::io_context& ioc, Awaitable&& awaitable) {
    using ResultType = decltype(std::declval<Awaitable>().await_resume());
    struct VoidResult {};
    using StoredType = std::conditional_t<std::is_void_v<ResultType>, VoidResult, ResultType>;
    std::optional<StoredType> result;
    std::exception_ptr exception;

    asio::co_spawn(ioc, [&]() -> asio::awaitable<void> {
        try {
            if constexpr (std::is_void_v<ResultType>) {
                co_await std::forward<Awaitable>(awaitable);
                result = VoidResult{};
            } else {
                result = co_await std::forward<Awaitable>(awaitable);
            }
        } catch (...) {
            exception = std::current_exception();
        }
    }, asio::detached);

    ioc.run();
    ioc.restart();

    if (exception) {
        std::rethrow_exception(exception);
    }
    if constexpr (!std::is_void_v<ResultType>) {
        return std::move(*result);
    }
}

TEST_CASE("ConnectionProfileRepository") {
    asio::io_context ioc;

    auto storage = std::make_shared<ConnectionProfileRepository::Storage>(
        DBConfiguration::createStorage(":memory:")
    );
    storage->sync_schema();

    ConnectionProfileRepository repo(ioc, storage);
    boost::uuids::random_generator uuid_gen;

    auto create_test_profile = [&](std::string server_url = "ws://localhost:9001",
                                   std::string stun_url = "stun:stun.l.google.com:19302",
                                   std::chrono::system_clock::time_point updated_at = std::chrono::system_clock::now())
        -> ConnectionProfileModel {
        return ConnectionProfileModel(
            uuid_gen(),
            std::move(server_url),
            std::move(stun_url),
            updated_at
        );
    };

    // Очищаем таблицу перед каждым тестом
    storage->remove_all<ConnectionProfileDAO>();

    SUBCASE("addProfile inserts profile successfully") {
        auto profile = create_test_profile();
        auto dao = ConnectionProfileDAOConverter::convert(profile);
        run_async(ioc, repo.addProfile(dao));

        auto all = run_async(ioc, repo.getAllProfiles());
        CHECK(all.size() == 1);
        CHECK(all[0].getId() == profile.getId());
    }

    SUBCASE("addProfile throws AlreadyExistsError when profile with same id exists") {
        auto profile = create_test_profile();
        auto dao = ConnectionProfileDAOConverter::convert(profile);
        run_async(ioc, repo.addProfile(dao));

        CHECK_THROWS_AS(run_async(ioc, repo.addProfile(dao)),
                        ConnectionProfileAlreadyExistsError);
    }

    SUBCASE("getProfileById returns profile when exists") {
        auto profile = create_test_profile();
        auto dao = ConnectionProfileDAOConverter::convert(profile);
        run_async(ioc, repo.addProfile(dao));

        auto retrieved = run_async(ioc, repo.getProfileById(profile.getId()));
        CHECK(retrieved.getId() == profile.getId());
        CHECK(retrieved.getServerUrl() == profile.getServerUrl());
    }

    SUBCASE("getProfileById throws NotFoundError when profile does not exist") {
        auto non_existent_id = uuid_gen();
        CHECK_THROWS_AS(run_async(ioc, repo.getProfileById(non_existent_id)),
                        ConnectionProfileNotFoundError);
    }

    SUBCASE("updateProfile modifies existing profile") {
        auto profile = create_test_profile("ws://old");
        auto dao = ConnectionProfileDAOConverter::convert(profile);
        run_async(ioc, repo.addProfile(dao));

        profile.setServerUrl("ws://new");
        auto updated_dao = ConnectionProfileDAOConverter::convert(profile);
        run_async(ioc, repo.updateProfile(updated_dao));

        auto retrieved = run_async(ioc, repo.getProfileById(profile.getId()));
        CHECK(retrieved.getServerUrl() == "ws://new");
    }

    SUBCASE("updateProfile throws NotFoundError when profile does not exist") {
        auto profile = create_test_profile();
        auto dao = ConnectionProfileDAOConverter::convert(profile);
        CHECK_THROWS_AS(run_async(ioc, repo.updateProfile(dao)),
                        ConnectionProfileNotFoundError);
    }

    SUBCASE("removeProfile deletes existing profile") {
        auto profile = create_test_profile();
        auto dao = ConnectionProfileDAOConverter::convert(profile);
        run_async(ioc, repo.addProfile(dao));
        CHECK(run_async(ioc, repo.getAllProfiles()).size() == 1);

        run_async(ioc, repo.removeProfile(profile.getId()));
        CHECK(run_async(ioc, repo.getAllProfiles()).empty());
    }

    SUBCASE("removeProfile does not throw when profile does not exist") {
        auto non_existent_id = uuid_gen();
        CHECK_NOTHROW(run_async(ioc, repo.removeProfile(non_existent_id)));
    }

    SUBCASE("getAllProfiles returns profiles sorted by updated_at desc") {
        using namespace std::chrono_literals;
        auto now = std::chrono::system_clock::now();

        auto p1 = create_test_profile("url1", "stun1", now - 20s);
        auto p2 = create_test_profile("url2", "stun2", now);
        auto p3 = create_test_profile("url3", "stun3", now - 10s);

        run_async(ioc, repo.addProfile(ConnectionProfileDAOConverter::convert(p1)));
        run_async(ioc, repo.addProfile(ConnectionProfileDAOConverter::convert(p2)));
        run_async(ioc, repo.addProfile(ConnectionProfileDAOConverter::convert(p3)));

        auto result = run_async(ioc, repo.getAllProfiles(true));
        REQUIRE(result.size() == 3);

        // Проверяем сортировку по убыванию updated_at
        CHECK(result[0].getId() == p2.getId()); // самый свежий
        CHECK(result[1].getId() == p3.getId());
        CHECK(result[2].getId() == p1.getId());
    }

    SUBCASE("getAllProfiles returns empty list when no profiles") {
        auto all = run_async(ioc, repo.getAllProfiles());
        CHECK(all.empty());
    }

    SUBCASE("getLatestProfile returns most recently updated profile") {
        using namespace std::chrono_literals;
        auto now = std::chrono::system_clock::now();

        auto p1 = create_test_profile("url1", "stun1", now - 20s);
        auto p2 = create_test_profile("url2", "stun2", now);

        run_async(ioc, repo.addProfile(ConnectionProfileDAOConverter::convert(p1)));
        run_async(ioc, repo.addProfile(ConnectionProfileDAOConverter::convert(p2)));

        auto latest = run_async(ioc, repo.getLatestProfile());
        CHECK(latest.getId() == p2.getId());
    }

    SUBCASE("getLatestProfile throws NotFoundError when no profiles exist") {
        CHECK_THROWS_AS(run_async(ioc, repo.getLatestProfile()),
                        ConnectionProfileNotFoundError);
    }
}
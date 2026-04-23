#include <doctest/doctest.h>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <chrono>
#include <vector>
#include <algorithm>

#include "UserService.h"
#include "UserRepository.h"
#include "UserExceptions.h"
#include "UserModel.h"
#include "UserDAOConverter.h"
#include "UserDAO.h"
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

TEST_CASE("UserService") {
    asio::io_context ioc;

    auto storage = std::make_shared<UserRepository::Storage>(
        DBConfiguration::createStorage(":memory:")
    );
    storage->sync_schema();

    auto repo = std::make_shared<UserRepository>(ioc, storage);
    UserService service(repo);
    boost::uuids::random_generator uuid_gen;

    // Вспомогательная функция для создания тестового пользователя
    auto create_test_user = [&](std::string login = "testuser",
                                std::string password_hash = "hash",
                                std::optional<boost::uuids::uuid> member_id = std::nullopt,
                                std::optional<std::chrono::system_clock::time_point> created_at = std::nullopt,
                                std::optional<std::chrono::system_clock::time_point> updated_at = std::nullopt)
        -> UserModel {
        auto now = std::chrono::system_clock::now();
        boost::uuids::uuid mid = member_id.value_or(uuid_gen());
        return UserModel(
            std::move(login),
            std::move(password_hash),
            mid,
            created_at.value_or(now),
            updated_at.value_or(now)
        );
    };

    // Очищаем таблицу перед каждым тестом
    storage->remove_all<UserDAO>();

    SUBCASE("getAllUsers returns multiple users") {
        auto u1 = create_test_user("alice");
        auto u2 = create_test_user("bob");
        run_async(ioc, service.addUser(u1));
        run_async(ioc, service.addUser(u2));

        auto all = run_async(ioc, service.getAllUsers());
        CHECK(all.size() == 2);
        std::vector<std::string> logins;
        for (const auto& u : all) logins.push_back(u.getLogin());
        CHECK(std::find(logins.begin(), logins.end(), "alice") != logins.end());
        CHECK(std::find(logins.begin(), logins.end(), "bob") != logins.end());
    }

    SUBCASE("getAllUsers returns empty list when no users") {
        auto all = run_async(ioc, service.getAllUsers());
        CHECK(all.empty());
    }

    SUBCASE("getUserByLogin returns user when exists") {
        auto user = create_test_user("alice");
        run_async(ioc, service.addUser(user));

        auto retrieved = run_async(ioc, service.getUserByLogin("alice"));
        CHECK(retrieved.getLogin() == user.getLogin());
        CHECK(retrieved.getMemberId() == user.getMemberId());
    }

    SUBCASE("getUserByLogin throws NotFoundError when user does not exist") {
        CHECK_THROWS_AS(run_async(ioc, service.getUserByLogin("nobody")),
                        UserNotFoundError);
    }

    SUBCASE("getUserByMemberId returns user when exists") {
        auto member_id = uuid_gen();
        auto user = create_test_user("alice", "hash", member_id);
        run_async(ioc, service.addUser(user));

        auto retrieved = run_async(ioc, service.getUserByMemberId(member_id));
        CHECK(retrieved.getLogin() == "alice");
    }

    SUBCASE("getUserByMemberId throws NotFoundError when member_id not found") {
        auto non_existent = uuid_gen();
        CHECK_THROWS_AS(run_async(ioc, service.getUserByMemberId(non_existent)),
                        UserNotFoundError);
    }

    SUBCASE("addUser inserts user successfully") {
        auto user = create_test_user();
        run_async(ioc, service.addUser(user));

        auto all = run_async(ioc, service.getAllUsers());
        CHECK(all.size() == 1);
        CHECK(all[0].getLogin() == user.getLogin());
    }

    SUBCASE("addUser throws AlreadyExistsError when login already exists") {
        auto user = create_test_user("alice");
        run_async(ioc, service.addUser(user));

        auto duplicate = create_test_user("alice", "otherhash");
        CHECK_THROWS_AS(run_async(ioc, service.addUser(duplicate)),
                        UserAlreadyExistsError);
    }

    SUBCASE("updateUser modifies existing user") {
        auto user = create_test_user("alice", "oldhash");
        run_async(ioc, service.addUser(user));

        user.setPasswordHash("newhash");
        run_async(ioc, service.updateUser(user));

        auto retrieved = run_async(ioc, service.getUserByLogin("alice"));
        CHECK(retrieved.getPasswordHash() == "newhash");
    }

    SUBCASE("updateUser throws NotFoundError when user does not exist") {
        auto user = create_test_user("ghost");
        CHECK_THROWS_AS(run_async(ioc, service.updateUser(user)),
                        UserNotFoundError);
    }

    SUBCASE("removeUser deletes existing user") {
        auto user = create_test_user("todelete");
        run_async(ioc, service.addUser(user));
        CHECK(run_async(ioc, service.getAllUsers()).size() == 1);

        run_async(ioc, service.removeUser("todelete"));
        CHECK(run_async(ioc, service.getAllUsers()).empty());
    }

    SUBCASE("removeUser does not throw when user does not exist") {
        CHECK_NOTHROW(run_async(ioc, service.removeUser("nobody")));
    }
}
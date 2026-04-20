#include <doctest/doctest.h>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <chrono>
#include <vector>
#include <algorithm>

#include "ChatMemberRepository.h"
#include "ChatMemberExceptions.h"
#include "ChatMemberModel.h"
#include "ChatMemberDAOConverter.h"
#include "ChatMemberDAO.h"
#include "DBConfiguration.h"

namespace asio = boost::asio;

// Вспомогательная функция для запуска корутины в тестовом io_context
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

TEST_CASE("ChatMemberRepository") {
    asio::io_context ioc;

    auto storage = std::make_shared<ChatMemberRepository::Storage>(
        DBConfiguration::createStorage(":memory:")
    );
    storage->sync_schema();

    // Также нужно создать таблицу chats для FK (чтобы вставка chat_members не падала)
    storage->get_all<ChatDAO>(); // принудительно создаём все таблицы

    ChatMemberRepository repo(ioc, storage);
    boost::uuids::random_generator uuid_gen;

    // Вспомогательная функция для создания тестового участника
    auto create_test_member = [&](std::string username = "test_user",
                                  std::optional<std::chrono::system_clock::time_point> last_online = std::nullopt)
        -> ChatMemberModel {
        return ChatMemberModel(
            uuid_gen(),            // id
            uuid_gen(),            // chat_id (произвольный, FK в памяти не проверяется активно)
            std::move(username),
            last_online
        );
    };

    // Перед каждым тестом очищаем таблицы (чтобы изолировать тесты)
    auto clear_tables = [&]() {
        storage->remove_all<ChatMemberDAO>();
        storage->remove_all<ChatDAO>();
    };
    clear_tables();

    // Чтобы FK не ругался, вставим фиктивный чат
    ChatDAO dummy_chat(uuid_gen(), uuid_gen(), ChatType::Direct, "dummy",
                       std::chrono::system_clock::now(), 0);
    storage->replace(dummy_chat);

    SUBCASE("getMemberById returns member when exists") {
        auto member = create_test_member();
        auto dao = ChatMemberDAOConverter::convert(member);
        run_async(ioc, repo.addMember(dao));

        auto retrieved = run_async(ioc, repo.getMemberById(member.getId()));
        CHECK(retrieved.getId() == member.getId());
        CHECK(retrieved.getUsername() == member.getUsername());
    }

    SUBCASE("getMemberById throws NotFoundError when member does not exist") {
        auto non_existent_id = uuid_gen();
        CHECK_THROWS_AS(run_async(ioc, repo.getMemberById(non_existent_id)),
                        ChatMemberNotFoundError);
    }

    SUBCASE("addMember inserts member successfully") {
        auto member = create_test_member();
        auto dao = ChatMemberDAOConverter::convert(member);
        run_async(ioc, repo.addMember(dao));

        auto all = run_async(ioc, repo.getAllMembers());
        CHECK(all.size() == 1);
        CHECK(all[0].getId() == member.getId());
    }

    SUBCASE("addMember throws AlreadyExistsError when member with same id exists") {
        auto member = create_test_member();
        auto dao = ChatMemberDAOConverter::convert(member);
        run_async(ioc, repo.addMember(dao));

        CHECK_THROWS_AS(run_async(ioc, repo.addMember(dao)),
                        ChatMemberAlreadyExistsError);
    }

    SUBCASE("updateMember modifies existing member") {
        auto member = create_test_member("original");
        auto dao = ChatMemberDAOConverter::convert(member);
        run_async(ioc, repo.addMember(dao));

        member.setUsername("updated");
        auto updated_dao = ChatMemberDAOConverter::convert(member);
        run_async(ioc, repo.updateMember(updated_dao));

        auto retrieved = run_async(ioc, repo.getMemberById(member.getId()));
        CHECK(retrieved.getUsername() == "updated");
    }

    SUBCASE("updateMember throws NotFoundError when member does not exist") {
        auto member = create_test_member();
        auto dao = ChatMemberDAOConverter::convert(member);
        CHECK_THROWS_AS(run_async(ioc, repo.updateMember(dao)),
                        ChatMemberNotFoundError);
    }

    SUBCASE("removeMember deletes existing member") {
        auto member = create_test_member();
        auto dao = ChatMemberDAOConverter::convert(member);
        run_async(ioc, repo.addMember(dao));
        CHECK(run_async(ioc, repo.getAllMembers()).size() == 1);

        run_async(ioc, repo.removeMember(member.getId()));
        CHECK(run_async(ioc, repo.getAllMembers()).empty());
    }

    SUBCASE("removeMember does not throw when member does not exist") {
        auto non_existent_id = uuid_gen();
        CHECK_NOTHROW(run_async(ioc, repo.removeMember(non_existent_id)));
    }

    SUBCASE("getAllMembers returns multiple members") {
        auto m1 = create_test_member("user1");
        auto m2 = create_test_member("user2");
        run_async(ioc, repo.addMember(ChatMemberDAOConverter::convert(m1)));
        run_async(ioc, repo.addMember(ChatMemberDAOConverter::convert(m2)));

        auto all = run_async(ioc, repo.getAllMembers());
        CHECK(all.size() == 2);
    }

    SUBCASE("getAllMembers returns empty list when no members") {
        auto all = run_async(ioc, repo.getAllMembers());
        CHECK(all.empty());
    }

    SUBCASE("getMembersByUsername returns multiple members sorted by last_online_at desc") {
        using namespace std::chrono_literals;
        auto now = std::chrono::system_clock::now();

        auto m1 = create_test_member("alice", now - 10s);   // старше
        auto m2 = create_test_member("alice", now);         // новее
        auto m3 = create_test_member("alice", now - 20s);   // ещё старше

        run_async(ioc, repo.addMember(ChatMemberDAOConverter::convert(m1)));
        run_async(ioc, repo.addMember(ChatMemberDAOConverter::convert(m2)));
        run_async(ioc, repo.addMember(ChatMemberDAOConverter::convert(m3)));

        auto result = run_async(ioc, repo.getMembersByUsername("alice", true));
        REQUIRE(result.size() == 3);

        auto unix_now = TimePointConverter::toUnixSeconds(now);
        auto unix_m2 = TimePointConverter::toUnixSeconds(*m2.getLastOnlineAt());
        auto unix_m1 = TimePointConverter::toUnixSeconds(*m1.getLastOnlineAt());
        auto unix_m3 = TimePointConverter::toUnixSeconds(*m3.getLastOnlineAt());
        // Проверяем сортировку по убыванию (сначала самый свежий)
        CHECK(TimePointConverter::toUnixSeconds(*result[0].getLastOnlineAt()) == unix_m2);
        CHECK(TimePointConverter::toUnixSeconds(*result[1].getLastOnlineAt()) == unix_m1);
        CHECK(TimePointConverter::toUnixSeconds(*result[2].getLastOnlineAt()) == unix_m3);
    }

    SUBCASE("getMembersByUsername returns empty list for non-existent username") {
        auto result = run_async(ioc, repo.getMembersByUsername("nobody"));
        CHECK(result.empty());
    }
}
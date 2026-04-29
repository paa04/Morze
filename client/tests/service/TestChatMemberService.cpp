#include <doctest/doctest.h>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <chrono>
#include <vector>
#include <algorithm>

#include "ChatMemberService.h"
#include "ChatMemberRepository.h"
#include "ChatMemberExceptions.h"
#include "ChatMemberModel.h"
#include "ChatMemberDAO.h"
#include "ChatDAO.h"
#include "ChatMemberRelationDAO.h"
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

TEST_CASE("ChatMemberService") {
    asio::io_context ioc;

    auto storage = std::make_shared<ChatMemberRepository::Storage>(
        DBConfiguration::createStorage(":memory:")
    );
    storage->sync_schema();

    auto repo = std::make_shared<ChatMemberRepository>(ioc, storage);
    ChatMemberService service(repo);
    boost::uuids::random_generator uuid_gen;

    // Вспомогательная функция для создания тестового участника
    auto create_test_member = [&](std::string username = "test_user",
                                  std::optional<std::chrono::system_clock::time_point> last_online = std::nullopt)
        -> ChatMemberModel {
        return ChatMemberModel(std::move(username), last_online);
    };

    // Очищаем таблицы перед каждым тестом
    auto clear_tables = [&]() {
        storage->remove_all<ChatMemberRelationDAO>();
        storage->remove_all<ChatMemberDAO>();
        storage->remove_all<ChatDAO>();
    };
    clear_tables();

    SUBCASE("getAllMembers returns multiple members sorted by last_online_at desc") {
        using namespace std::chrono_literals;
        auto now = std::chrono::system_clock::now();

        auto m1 = create_test_member("alice", now - 10s);
        auto m2 = create_test_member("bob", now);
        auto m3 = create_test_member("charlie", now - 20s);

        run_async(ioc, service.addMember(m1));
        run_async(ioc, service.addMember(m2));
        run_async(ioc, service.addMember(m3));

        auto result = run_async(ioc, service.getAllMembers(true));
        REQUIRE(result.size() == 3);
        CHECK(result[0].getId() == m2.getId());
        CHECK(result[1].getId() == m1.getId());
        CHECK(result[2].getId() == m3.getId());
    }

    SUBCASE("getAllMembers returns empty list when no members") {
        auto all = run_async(ioc, service.getAllMembers());
        CHECK(all.empty());
    }

    SUBCASE("getMemberById returns member when exists") {
        auto member = create_test_member();
        run_async(ioc, service.addMember(member));

        auto retrieved = run_async(ioc, service.getMemberById(member.getId()));
        CHECK(retrieved.getId() == member.getId());
        CHECK(retrieved.getUsername() == member.getUsername());
    }

    SUBCASE("getMemberById throws NotFoundError when member does not exist") {
        auto non_existent_id = uuid_gen();
        CHECK_THROWS_AS(run_async(ioc, service.getMemberById(non_existent_id)),
                        ChatMemberNotFoundError);
    }

    SUBCASE("addMember inserts member successfully") {
        auto member = create_test_member();
        run_async(ioc, service.addMember(member));

        auto all = run_async(ioc, service.getAllMembers());
        CHECK(all.size() == 1);
        CHECK(all[0].getId() == member.getId());
    }

    SUBCASE("addMember throws AlreadyExistsError when member with same id exists") {
        auto member = create_test_member();
        run_async(ioc, service.addMember(member));

        CHECK_THROWS_AS(run_async(ioc, service.addMember(member)),
                        ChatMemberAlreadyExistsError);
    }

    SUBCASE("updateMember modifies existing member") {
        auto member = create_test_member("original");
        run_async(ioc, service.addMember(member));

        member.setUsername("updated");
        run_async(ioc, service.updateMember(member));

        auto retrieved = run_async(ioc, service.getMemberById(member.getId()));
        CHECK(retrieved.getUsername() == "updated");
    }

    SUBCASE("updateMember throws NotFoundError when member does not exist") {
        auto member = create_test_member();
        CHECK_THROWS_AS(run_async(ioc, service.updateMember(member)),
                        ChatMemberNotFoundError);
    }

    SUBCASE("removeMember deletes existing member") {
        auto member = create_test_member();
        run_async(ioc, service.addMember(member));
        CHECK(run_async(ioc, service.getAllMembers()).size() == 1);

        run_async(ioc, service.removeMember(member.getId()));
        CHECK(run_async(ioc, service.getAllMembers()).empty());
    }

    SUBCASE("removeMember does not throw when member does not exist") {
        auto non_existent_id = uuid_gen();
        CHECK_NOTHROW(run_async(ioc, service.removeMember(non_existent_id)));
    }

    SUBCASE("getMembersByChatId returns members for given chat") {
        // Создаём чат и участников
        boost::uuids::uuid chat_id = uuid_gen();
        ChatDAO chat(chat_id, uuid_gen(), ChatType::Direct, "Test Chat",
                     std::chrono::system_clock::now(), 0);
        storage->replace(chat);

        auto m1 = create_test_member("alice");
        auto m2 = create_test_member("bob");
        run_async(ioc, service.addMember(m1));
        run_async(ioc, service.addMember(m2));

        // Связываем участников с чатом через репозиторий чата (или напрямую через relation)
        ChatMemberRelationDAO rel1(uuid_gen(), chat_id, m1.getId());
        ChatMemberRelationDAO rel2(uuid_gen(), chat_id, m2.getId());
        storage->replace(rel1);
        storage->replace(rel2);

        auto result = run_async(ioc, service.getMembersByChatId(chat_id));
        REQUIRE(result.size() == 2);
        std::vector<boost::uuids::uuid> ids;
        for (const auto& m : result) ids.push_back(m.getId());
        CHECK(std::find(ids.begin(), ids.end(), m1.getId()) != ids.end());
        CHECK(std::find(ids.begin(), ids.end(), m2.getId()) != ids.end());
    }

    SUBCASE("getMembersByChatId returns empty list for chat with no members") {
        boost::uuids::uuid empty_chat_id = uuid_gen();
        ChatDAO empty_chat(empty_chat_id, uuid_gen(), ChatType::Direct, "Empty",
                           std::chrono::system_clock::now(), 0);
        storage->replace(empty_chat);

        auto result = run_async(ioc, service.getMembersByChatId(empty_chat_id));
        CHECK(result.empty());
    }
}
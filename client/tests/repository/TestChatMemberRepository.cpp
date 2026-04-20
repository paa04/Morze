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
#include "ChatDAO.h"
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

TEST_CASE("ChatMemberRepository") {
    asio::io_context ioc;

    auto storage = std::make_shared<ChatMemberRepository::Storage>(
        DBConfiguration::createStorage(":memory:")
    );
    storage->sync_schema();

    ChatMemberRepository repo(ioc, storage);
    boost::uuids::random_generator uuid_gen;

    // Фиктивные чаты для внешнего ключа
    boost::uuids::uuid dummy_chat_id = uuid_gen();
    boost::uuids::uuid dummy_chat_id_2 = uuid_gen();
    ChatDAO dummy_chat(dummy_chat_id, uuid_gen(), ChatType::Direct, "dummy",
                       std::chrono::system_clock::now(), 0);
    storage->replace(dummy_chat);
    ChatDAO dummy_chat_2(dummy_chat_id_2, uuid_gen(), ChatType::Direct, "dummy2",
                         std::chrono::system_clock::now(), 0);
    storage->replace(dummy_chat_2);

    // Вспомогательная функция для создания тестового участника
    // Принимает username, chat_id, user_id (если не указан — генерируется новый), и last_online
    auto create_test_member = [&](
        std::string username,
        boost::uuids::uuid chat_id,
        std::optional<boost::uuids::uuid> user_id = std::nullopt,
        std::optional<std::chrono::system_clock::time_point> last_online = std::nullopt)
        -> ChatMemberModel {
        auto uid = user_id.value_or(uuid_gen());
        return ChatMemberModel(
            chat_id,
            uid,
            std::move(username),
            last_online
        );
    };

    // Очищаем таблицы перед каждым тестом
    auto clear_tables = [&]() {
        storage->remove_all<ChatMemberDAO>();
        // Чат не удаляем, он нужен для FK
    };
    clear_tables();

    SUBCASE("getAllMembers returns multiple members sorted by last_online_at desc") {
        using namespace std::chrono_literals;
        auto now = std::chrono::system_clock::now();

        auto m1 = create_test_member("alice", dummy_chat_id, std::nullopt, now - 10s);
        auto m2 = create_test_member("bob", dummy_chat_id, std::nullopt, now);
        auto m3 = create_test_member("charlie", dummy_chat_id, std::nullopt, now - 20s);

        run_async(ioc, repo.addMember(ChatMemberDAOConverter::convert(m1)));
        run_async(ioc, repo.addMember(ChatMemberDAOConverter::convert(m2)));
        run_async(ioc, repo.addMember(ChatMemberDAOConverter::convert(m3)));

        auto result = run_async(ioc, repo.getAllMembers(true));
        REQUIRE(result.size() == 3);
        // проверяем сортировку: сначала самый свежий (m2), потом m1, потом m3
        CHECK(result[0].getUserId() == m2.getUserId());
        CHECK(result[1].getUserId() == m1.getUserId());
        CHECK(result[2].getUserId() == m3.getUserId());
    }

    SUBCASE("getAllMembers returns empty list when no members") {
        auto all = run_async(ioc, repo.getAllMembers());
        CHECK(all.empty());
    }

    SUBCASE("getMembersByUserId returns members with given user_id") {
        boost::uuids::uuid common_user_id = uuid_gen();
        // Создаём двух участников с одинаковым user_id в разных чатах
        ChatMemberModel m1(dummy_chat_id, common_user_id, "user1", std::nullopt);
        ChatMemberModel m2(dummy_chat_id_2, common_user_id, "user2", std::nullopt);
        ChatMemberModel m3(dummy_chat_id, uuid_gen(), "user3", std::nullopt); // другой user_id

        run_async(ioc, repo.addMember(ChatMemberDAOConverter::convert(m1)));
        run_async(ioc, repo.addMember(ChatMemberDAOConverter::convert(m2)));
        run_async(ioc, repo.addMember(ChatMemberDAOConverter::convert(m3)));

        auto result = run_async(ioc, repo.getMembersByUserId(common_user_id));
        CHECK(result.size() == 2);
        // порядок не гарантирован, проверим наличие
        std::vector<boost::uuids::uuid> ids;
        for (const auto& m : result) ids.push_back(m.getUserId());
        CHECK(std::find(ids.begin(), ids.end(), m1.getUserId()) != ids.end());
        CHECK(std::find(ids.begin(), ids.end(), m2.getUserId()) != ids.end());
    }

    SUBCASE("getMembersByUserId returns empty vector when no member found") {
        auto non_existent = uuid_gen();
        auto result = run_async(ioc, repo.getMembersByUserId(non_existent));
        CHECK(result.empty());
    }

    SUBCASE("addMember inserts member successfully") {
        auto member = create_test_member("member", dummy_chat_id);
        auto dao = ChatMemberDAOConverter::convert(member);
        run_async(ioc, repo.addMember(dao));

        auto all = run_async(ioc, repo.getAllMembers());
        CHECK(all.size() == 1);
        CHECK(all[0].getUserId() == member.getUserId());
        CHECK(all[0].getChatId() == member.getChatId());
    }

    SUBCASE("addMember throws AlreadyExistsError when member with same (chat_id, user_id) exists") {
        auto member = create_test_member("member", dummy_chat_id);
        auto dao = ChatMemberDAOConverter::convert(member);
        run_async(ioc, repo.addMember(dao));

        // Попытка добавить того же пользователя в тот же чат
        CHECK_THROWS_AS(run_async(ioc, repo.addMember(dao)),
                        ChatMemberAlreadyExistsError);
    }

    SUBCASE("updateMember modifies existing member") {
        auto member = create_test_member("original", dummy_chat_id);
        auto dao = ChatMemberDAOConverter::convert(member);
        run_async(ioc, repo.addMember(dao));

        member.setUsername("updated");
        auto updated_dao = ChatMemberDAOConverter::convert(member);
        run_async(ioc, repo.updateMember(updated_dao));

        // получаем всех, находим нужного
        auto all = run_async(ioc, repo.getAllMembers());
        REQUIRE(all.size() == 1);
        CHECK(all[0].getUsername() == "updated");
    }

    SUBCASE("updateMember throws NotFoundError when member does not exist") {
        auto member = create_test_member("ghost", dummy_chat_id);
        auto dao = ChatMemberDAOConverter::convert(member);
        CHECK_THROWS_AS(run_async(ioc, repo.updateMember(dao)),
                        ChatMemberNotFoundError);
    }

    SUBCASE("removeMember deletes existing member") {
        auto member = create_test_member("todelete", dummy_chat_id);
        auto dao = ChatMemberDAOConverter::convert(member);
        run_async(ioc, repo.addMember(dao));
        CHECK(run_async(ioc, repo.getAllMembers()).size() == 1);

        run_async(ioc, repo.removeMember(member.getChatId(), member.getUserId()));
        CHECK(run_async(ioc, repo.getAllMembers()).empty());
    }

    SUBCASE("removeMember does not throw when member does not exist") {
        auto non_existent_user = uuid_gen();
        CHECK_NOTHROW(run_async(ioc, repo.removeMember(dummy_chat_id, non_existent_user)));
    }
}
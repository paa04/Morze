#include <doctest/doctest.h>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <chrono>
#include <vector>
#include <algorithm>

#include "ChatService.h"
#include "ChatRepository.h"
#include "ChatMemberRepository.h"
#include "ChatExcpetions.h"
#include "ChatMemberExceptions.h"
#include "ChatModel.h"
#include "ChatDAOConverter.h"
#include "ChatMemberModel.h"
#include "ChatMemberDAOConverter.h"
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

TEST_CASE("ChatService") {
    asio::io_context ioc;

    auto storage = std::make_shared<ChatRepository::Storage>(
        DBConfiguration::createStorage(":memory:")
    );
    storage->sync_schema();

    auto repo = std::make_shared<ChatRepository>(ioc, storage);
    ChatService service(repo);
    boost::uuids::random_generator uuid_gen;

    // Вспомогательная функция для создания тестового чата
    auto create_test_chat = [&](std::string title = "Test Chat") -> ChatModel {
        return ChatModel(
            uuid_gen(),
            uuid_gen(),
            ChatType::Direct,
            std::move(title),
            std::chrono::system_clock::now(),
            0
        );
    };

    // Очищаем таблицы перед каждым тестом
    auto clear_tables = [&]() {
        storage->remove_all<ChatMemberRelationDAO>();
        storage->remove_all<ChatMemberDAO>();
        storage->remove_all<ChatDAO>();
    };
    clear_tables();

    SUBCASE("getAllChats returns non-empty list after insertion") {
        auto chat = create_test_chat();
        run_async(ioc, service.addChat(chat));

        auto all = run_async(ioc, service.getAllChats());
        CHECK(all.size() == 1);
        CHECK(all[0].getId() == chat.getId());
        CHECK(all[0].getTitle() == chat.getTitle());
    }

    SUBCASE("getAllChats returns empty list when no chats") {
        auto all = run_async(ioc, service.getAllChats());
        CHECK(all.empty());
    }

    SUBCASE("getChatById returns chat when exists") {
        auto chat = create_test_chat();
        run_async(ioc, service.addChat(chat));

        auto retrieved = run_async(ioc, service.getChatById(chat.getId()));
        CHECK(retrieved.getId() == chat.getId());
        CHECK(retrieved.getTitle() == chat.getTitle());
    }

    SUBCASE("getChatById throws NotFoundError when chat does not exist") {
        auto non_existent_id = uuid_gen();
        CHECK_THROWS_AS(run_async(ioc, service.getChatById(non_existent_id)),
                        ChatNotFoundError);
    }

    SUBCASE("addChat inserts chat successfully") {
        auto chat = create_test_chat();
        run_async(ioc, service.addChat(chat));

        auto all = run_async(ioc, service.getAllChats());
        CHECK(all.size() == 1);
        CHECK(all[0].getId() == chat.getId());
    }

    SUBCASE("addChat throws AlreadyExistsError when chat with same id exists") {
        auto chat = create_test_chat();
        run_async(ioc, service.addChat(chat));

        CHECK_THROWS_AS(run_async(ioc, service.addChat(chat)),
                        ChatAlreadyExistsError);
    }

    SUBCASE("updateChat modifies existing chat") {
        auto chat = create_test_chat("Original");
        run_async(ioc, service.addChat(chat));

        chat.setTitle("Updated");
        run_async(ioc, service.updateChat(chat));

        auto retrieved = run_async(ioc, service.getChatById(chat.getId()));
        CHECK(retrieved.getTitle() == "Updated");
    }

    SUBCASE("updateChat throws NotFoundError when chat does not exist") {
        auto chat = create_test_chat();
        CHECK_THROWS_AS(run_async(ioc, service.updateChat(chat)),
                        ChatNotFoundError);
    }

    SUBCASE("removeChat deletes existing chat") {
        auto chat = create_test_chat();
        run_async(ioc, service.addChat(chat));
        CHECK(run_async(ioc, service.getAllChats()).size() == 1);

        run_async(ioc, service.removeChat(chat.getId()));
        CHECK(run_async(ioc, service.getAllChats()).empty());
    }

    SUBCASE("removeChat does not throw when chat does not exist") {
        auto non_existent_id = uuid_gen();
        CHECK_NOTHROW(run_async(ioc, service.removeChat(non_existent_id)));
    }

    // Тесты для управления участниками чата
    SUBCASE("addMemberToChat and getChatsForMember") {
        auto chat = create_test_chat();
        run_async(ioc, service.addChat(chat));

        // Создаём участника через отдельный репозиторий
        ChatMemberRepository memberRepo(ioc, storage);
        ChatMemberModel member("alice", std::nullopt);
        run_async(ioc, memberRepo.addMember(ChatMemberDAOConverter::convert(member)));

        run_async(ioc, service.addMemberToChat(chat.getId(), member.getId()));

        auto chats = run_async(ioc, service.getChatsForMember(member.getId()));
        REQUIRE(chats.size() == 1);
        CHECK(chats[0].getId() == chat.getId());
    }

    SUBCASE("removeMemberFromChat removes relation") {
        auto chat = create_test_chat();
        run_async(ioc, service.addChat(chat));

        ChatMemberRepository memberRepo(ioc, storage);
        ChatMemberModel member("bob", std::nullopt);
        run_async(ioc, memberRepo.addMember(ChatMemberDAOConverter::convert(member)));

        run_async(ioc, service.addMemberToChat(chat.getId(), member.getId()));
        run_async(ioc, service.removeMemberFromChat(chat.getId(), member.getId()));

        auto chats = run_async(ioc, service.getChatsForMember(member.getId()));
        CHECK(chats.empty());
    }

    SUBCASE("getChatsForMember returns empty when no chats") {
        ChatMemberRepository memberRepo(ioc, storage);
        ChatMemberModel member("charlie", std::nullopt);
        run_async(ioc, memberRepo.addMember(ChatMemberDAOConverter::convert(member)));

        auto chats = run_async(ioc, service.getChatsForMember(member.getId()));
        CHECK(chats.empty());
    }

    SUBCASE("addMemberToChat throws when adding duplicate") {
        auto chat = create_test_chat();
        run_async(ioc, service.addChat(chat));

        ChatMemberRepository memberRepo(ioc, storage);
        ChatMemberModel member("dave", std::nullopt);
        run_async(ioc, memberRepo.addMember(ChatMemberDAOConverter::convert(member)));

        run_async(ioc, service.addMemberToChat(chat.getId(), member.getId()));
        CHECK_THROWS_AS(run_async(ioc, service.addMemberToChat(chat.getId(), member.getId())),
                        ChatMemberAlreadyExistsError);
    }
}
#include <doctest/doctest.h>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include "ChatRepository.h"
#include "ChatExcpetions.h"
#include "ChatModel.h"
#include "ChatDAOConverter.h"
#include "ChatMemberRepository.h"
#include "ChatMemberDAOConverter.h"
#include "ChatMemberRelationDAO.h"
#include "DBConfiguration.h"

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

TEST_CASE("ChatRepository") {
    asio::io_context ioc;

    auto storage = std::make_shared<ChatRepository::Storage>(
        DBConfiguration::createStorage(":memory:")
    );
    storage->sync_schema();

    ChatRepository repo(ioc, storage);
    boost::uuids::random_generator uuid_gen;

    // Вспомогательная функция для создания тестового чата
    auto create_test_chat = [&](std::string title = "Test") -> ChatModel {
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
        auto dao = ChatDAOConverter::convert(chat);
        run_async(ioc, repo.addChat(dao));

        auto all = run_async(ioc, repo.getAllChats());
        CHECK(all.size() == 1);
        CHECK(all[0].getId() == chat.getId());
        CHECK(all[0].getTitle() == chat.getTitle());
    }

    SUBCASE("getAllChats returns empty list when no chats") {
        auto all = run_async(ioc, repo.getAllChats());
        CHECK(all.empty());
    }

    SUBCASE("getChatById returns chat when exists") {
        auto chat = create_test_chat();
        auto dao = ChatDAOConverter::convert(chat);
        run_async(ioc, repo.addChat(dao));

        auto retrieved = run_async(ioc, repo.getChatById(chat.getId()));
        CHECK(retrieved.getId() == chat.getId());
        CHECK(retrieved.getTitle() == chat.getTitle());
    }

    SUBCASE("getChatById throws NotFoundError when chat does not exist") {
        auto non_existent_id = uuid_gen();
        CHECK_THROWS_AS(run_async(ioc, repo.getChatById(non_existent_id)), ChatNotFoundError);
    }

    SUBCASE("addChat inserts chat successfully") {
        auto chat = create_test_chat();
        auto dao = ChatDAOConverter::convert(chat);
        run_async(ioc, repo.addChat(dao));

        auto all = run_async(ioc, repo.getAllChats());
        CHECK(all.size() == 1);
        CHECK(all[0].getId() == chat.getId());
    }

    SUBCASE("addChat throws AlreadyExistsError when chat with same id exists") {
        auto chat = create_test_chat();
        auto dao = ChatDAOConverter::convert(chat);
        run_async(ioc, repo.addChat(dao));

        CHECK_THROWS_AS(run_async(ioc, repo.addChat(dao)), ChatAlreadyExistsError);
    }

    SUBCASE("updateChat modifies existing chat") {
        auto chat = create_test_chat("Original");
        auto dao = ChatDAOConverter::convert(chat);
        run_async(ioc, repo.addChat(dao));

        chat.setTitle("Updated");
        auto updated_dao = ChatDAOConverter::convert(chat);
        run_async(ioc, repo.updateChat(updated_dao));

        auto retrieved = run_async(ioc, repo.getChatById(chat.getId()));
        CHECK(retrieved.getTitle() == "Updated");
    }

    SUBCASE("updateChat throws NotFoundError when chat does not exist") {
        auto chat = create_test_chat();
        auto dao = ChatDAOConverter::convert(chat);
        CHECK_THROWS_AS(run_async(ioc, repo.updateChat(dao)), ChatNotFoundError);
    }

    SUBCASE("removeChat deletes existing chat") {
        auto chat = create_test_chat();
        auto dao = ChatDAOConverter::convert(chat);
        run_async(ioc, repo.addChat(dao));
        CHECK(run_async(ioc, repo.getAllChats()).size() == 1);

        run_async(ioc, repo.removeChat(chat.getId()));
        CHECK(run_async(ioc, repo.getAllChats()).empty());
    }

    SUBCASE("removeChat does not throw when chat does not exist") {
        auto non_existent_id = uuid_gen();
        CHECK_NOTHROW(run_async(ioc, repo.removeChat(non_existent_id)));
    }

    // Новые тесты для управления участниками чата
    SUBCASE("addMemberToChat and getChatsForMember") {
        // Создаём чат и участника
        auto chat = create_test_chat();
        auto chatDao = ChatDAOConverter::convert(chat);
        run_async(ioc, repo.addChat(chatDao));

        ChatMemberModel member("alice", std::nullopt);
        auto memberDao = ChatMemberDAOConverter::convert(member);
        // Нужно добавить участника через ChatMemberRepository, так как ChatRepository не управляет участниками
        ChatMemberRepository memberRepo(ioc, storage);
        run_async(ioc, memberRepo.addMember(memberDao));

        // Добавляем участника в чат
        run_async(ioc, repo.addMemberToChat(chat.getId(), member.getId()));

        // Проверяем, что чат появляется в списке чатов участника
        auto chats = run_async(ioc, repo.getChatsForMember(member.getId()));
        REQUIRE(chats.size() == 1);
        CHECK(chats[0].getId() == chat.getId());
    }

    SUBCASE("removeMemberFromChat removes relation") {
        auto chat = create_test_chat();
        auto chatDao = ChatDAOConverter::convert(chat);
        run_async(ioc, repo.addChat(chatDao));

        ChatMemberModel member("bob", std::nullopt);
        ChatMemberRepository memberRepo(ioc, storage);
        auto memberDao = ChatMemberDAOConverter::convert(member);
        run_async(ioc, memberRepo.addMember(memberDao));

        run_async(ioc, repo.addMemberToChat(chat.getId(), member.getId()));

        // Удаляем связь
        run_async(ioc, repo.removeMemberFromChat(chat.getId(), member.getId()));

        auto chats = run_async(ioc, repo.getChatsForMember(member.getId()));
        CHECK(chats.empty());
    }

    SUBCASE("getChatsForMember returns empty when no chats") {
        ChatMemberModel member("charlie", std::nullopt);
        ChatMemberRepository memberRepo(ioc, storage);
        auto memberDao = ChatMemberDAOConverter::convert(member);
        run_async(ioc, memberRepo.addMember(memberDao));

        auto chats = run_async(ioc, repo.getChatsForMember(member.getId()));
        CHECK(chats.empty());
    }
}
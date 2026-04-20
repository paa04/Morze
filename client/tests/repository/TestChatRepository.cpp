#include <doctest/doctest.h>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include "ChatRepository.h"
#include "ChatExcpetions.h"
#include "ChatModel.h"
#include "ChatDAOConverter.h"
#include "DBConfiguration.h"

namespace asio = boost::asio;

// Вспомогательная функция для запуска корутины в тестовом io_context и получения результата
template<typename Awaitable>
auto run_async(asio::io_context& ioc, Awaitable&& awaitable) {
    using ResultType = decltype(std::declval<Awaitable>().await_resume());

    struct VoidResult {}; // фиктивный тип для void
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
    // для void ничего не возвращаем
}

TEST_CASE("ChatRepository") {
    asio::io_context ioc;

    auto storage = std::make_shared<ChatRepository::Storage>(
        DBConfiguration::createStorage(":memory:")
    );
    storage->sync_schema();

    ChatRepository repo(ioc, storage);
    boost::uuids::random_generator uuid_gen;

    // Вспомогательная функция для создания тестового чата (модель)
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

    SUBCASE("getAll returns non-empty list after insertion") {
        auto chat = create_test_chat();
        auto dao = ChatDAOConverter::convert(chat);
        run_async(ioc, repo.addChat(dao));

        auto all = run_async(ioc, repo.getAllChats());
        CHECK(all.size() == 1);
        CHECK(all[0].getChatId() == chat.getChatId());
        CHECK(all[0].getTitle() == chat.getTitle());
    }

    SUBCASE("getAll returns empty list when no chats") {
        auto all = run_async(ioc, repo.getAllChats());
        CHECK(all.empty());
    }

    SUBCASE("getById returns chat when exists") {
        auto chat = create_test_chat();
        auto dao = ChatDAOConverter::convert(chat);
        run_async(ioc, repo.addChat(dao));

        auto retrieved = run_async(ioc, repo.getChatById(chat.getChatId()));
        CHECK(retrieved.getChatId() == chat.getChatId());
        CHECK(retrieved.getTitle() == chat.getTitle());
    }

    SUBCASE("getById throws NotFoundError when chat does not exist") {
        auto non_existent_id = uuid_gen();
        CHECK_THROWS_AS(run_async(ioc, repo.getChatById(non_existent_id)), ChatNotFoundError);
    }

    SUBCASE("addChat inserts chat successfully") {
        auto chat = create_test_chat();
        auto dao = ChatDAOConverter::convert(chat);
        run_async(ioc, repo.addChat(dao));

        auto all = run_async(ioc, repo.getAllChats());
        CHECK(all.size() == 1);
        CHECK(all[0].getChatId() == chat.getChatId());
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

        auto retrieved = run_async(ioc, repo.getChatById(chat.getChatId()));
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

        run_async(ioc, repo.removeChat(chat.getChatId()));
        CHECK(run_async(ioc, repo.getAllChats()).empty());
    }

    SUBCASE("removeChat does not throw when chat does not exist") {
        auto non_existent_id = uuid_gen();
        CHECK_NOTHROW(run_async(ioc, repo.removeChat(non_existent_id)));
    }
}
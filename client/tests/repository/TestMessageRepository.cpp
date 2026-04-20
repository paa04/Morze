#include <doctest/doctest.h>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <chrono>
#include <vector>
#include <algorithm>

#include "MessageRepository.h"
#include "MessageExceptions.h"
#include "MessageModel.h"
#include "MessageDAOConverter.h"
#include "MessageDAO.h"
#include "ChatDAO.h"            // для создания чата (FK)
#include "ChatMemberDAO.h"       // для создания отправителя (FK)
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

TEST_CASE("MessageRepository") {
    asio::io_context ioc;

    auto storage = std::make_shared<MessageRepository::Storage>(
        DBConfiguration::createStorage(":memory:")
    );
    storage->sync_schema();

    MessageRepository repo(ioc, storage);
    boost::uuids::random_generator uuid_gen;

    // Создаём фиктивные чат и участника для соблюдения FK
    boost::uuids::uuid chat_id = uuid_gen();
    boost::uuids::uuid sender_id = uuid_gen();

    ChatDAO chat(chat_id, uuid_gen(), ChatType::Direct, "Test Chat",
                 std::chrono::system_clock::now(), 0);
    storage->replace(chat);

    ChatMemberDAO sender(sender_id, chat_id, "sender", std::nullopt);
    storage->replace(sender);

    auto create_test_message = [&](std::string content,
                               boost::uuids::uuid cid,
                               boost::uuids::uuid sid,
                               std::chrono::system_clock::time_point created)
    -> MessageModel {
        return MessageModel(
            uuid_gen(),
            cid,
            sid,
            MessageDirection::Outgoing,
            std::move(content),
            created,
            DeliveryState::Sent
        );
    };

    // Очищаем таблицу сообщений перед каждым тестом
    storage->remove_all<MessageDAO>();

    SUBCASE("getMessageById returns message when exists") {
        auto msg = create_test_message("Hello", chat_id, sender_id, std::chrono::system_clock::now());
        auto dao = MessageDAOConverter::convert(msg);
        run_async(ioc, repo.addMessage(dao));

        auto retrieved = run_async(ioc, repo.getMessageById(msg.getMessageId()));
        CHECK(retrieved.getMessageId() == msg.getMessageId());
        CHECK(retrieved.getContent() == msg.getContent());
    }

    SUBCASE("getMessageById throws NotFoundError when message does not exist") {
        auto non_existent_id = uuid_gen();
        CHECK_THROWS_AS(run_async(ioc, repo.getMessageById(non_existent_id)),
                        MessageNotFoundError);
    }

    SUBCASE("getAllMessages returns all messages") {
        auto m1 = create_test_message("First", chat_id, sender_id, std::chrono::system_clock::now());
        auto m2 = create_test_message("Second", chat_id, sender_id, std::chrono::system_clock::now());
        run_async(ioc, repo.addMessage(MessageDAOConverter::convert(m1)));
        run_async(ioc, repo.addMessage(MessageDAOConverter::convert(m2)));

        auto all = run_async(ioc, repo.getAllMessages());
        CHECK(all.size() == 2);
    }

    SUBCASE("getAllMessages returns empty list when no messages") {
        auto all = run_async(ioc, repo.getAllMessages());
        CHECK(all.empty());
    }

    SUBCASE("updateMessage modifies existing message") {
        auto msg = create_test_message("Original", chat_id, sender_id, std::chrono::system_clock::now());
        auto dao = MessageDAOConverter::convert(msg);
        run_async(ioc, repo.addMessage(dao));

        msg.setContent("Updated");
        auto updated_dao = MessageDAOConverter::convert(msg);
        run_async(ioc, repo.updateMessage(updated_dao));

        auto retrieved = run_async(ioc, repo.getMessageById(msg.getMessageId()));
        CHECK(retrieved.getContent() == "Updated");
    }

    SUBCASE("updateMessage throws NotFoundError when message does not exist") {
        auto msg = create_test_message("Hello", chat_id, sender_id, std::chrono::system_clock::now());
        auto dao = MessageDAOConverter::convert(msg);
        CHECK_THROWS_AS(run_async(ioc, repo.updateMessage(dao)),
                        MessageNotFoundError);
    }

    SUBCASE("addMessage inserts message successfully") {
        auto msg = create_test_message("Hello", chat_id, sender_id, std::chrono::system_clock::now());
        auto dao = MessageDAOConverter::convert(msg);
        run_async(ioc, repo.addMessage(dao));

        auto all = run_async(ioc, repo.getAllMessages());
        CHECK(all.size() == 1);
        CHECK(all[0].getMessageId() == msg.getMessageId());
    }

    SUBCASE("addMessage throws AlreadyExistsError when message with same id exists") {
        auto msg = create_test_message("Hello", chat_id, sender_id, std::chrono::system_clock::now());
        auto dao = MessageDAOConverter::convert(msg);
        run_async(ioc, repo.addMessage(dao));

        CHECK_THROWS_AS(run_async(ioc, repo.addMessage(dao)),
                        MessageAlreadyExistsError);
    }

    SUBCASE("removeMessage deletes existing message") {
        auto msg = create_test_message("Hello", chat_id, sender_id, std::chrono::system_clock::now());
        auto dao = MessageDAOConverter::convert(msg);
        run_async(ioc, repo.addMessage(dao));
        CHECK(run_async(ioc, repo.getAllMessages()).size() == 1);

        run_async(ioc, repo.removeMessage(msg.getMessageId()));
        CHECK(run_async(ioc, repo.getAllMessages()).empty());
    }

    SUBCASE("removeMessage does not throw when message does not exist") {
        auto non_existent_id = uuid_gen();
        CHECK_NOTHROW(run_async(ioc, repo.removeMessage(non_existent_id)));
    }

    SUBCASE("getMessagesByChatId returns messages sorted by created_at desc") {
        using namespace std::chrono_literals;
        auto now = std::chrono::system_clock::now();

        auto m1 = create_test_message("Old", chat_id, sender_id, now - 20s);
        auto m2 = create_test_message("New", chat_id, sender_id, now);
        auto m3 = create_test_message("Mid", chat_id, sender_id, now - 10s);

        run_async(ioc, repo.addMessage(MessageDAOConverter::convert(m1)));
        run_async(ioc, repo.addMessage(MessageDAOConverter::convert(m2)));
        run_async(ioc, repo.addMessage(MessageDAOConverter::convert(m3)));

        auto result = run_async(ioc, repo.getMessagesByChatId(chat_id));
        REQUIRE(result.size() == 3);
        CHECK(result[0].getMessageId() == m2.getMessageId()); // newest
        CHECK(result[1].getMessageId() == m3.getMessageId());
        CHECK(result[2].getMessageId() == m1.getMessageId());
    }

    SUBCASE("getMessagesByChatId returns empty list for chat with no messages") {
        auto empty_chat_id = uuid_gen();
        ChatDAO empty_chat(empty_chat_id, uuid_gen(), ChatType::Direct, "Empty",
                           std::chrono::system_clock::now(), 0);
        storage->replace(empty_chat);
        auto result = run_async(ioc, repo.getMessagesByChatId(empty_chat_id));
        CHECK(result.empty());
    }

    SUBCASE("getMessagesBySenderInChat returns messages from sender in specific chat sorted") {
        using namespace std::chrono_literals;
        auto now = std::chrono::system_clock::now();
        auto other_sender_id = uuid_gen();
        ChatMemberDAO other_sender(other_sender_id, chat_id, "other", std::nullopt);
        storage->replace(other_sender);

        auto m1 = create_test_message("Sender Old", chat_id, sender_id, now - 20s);
        auto m2 = create_test_message("Sender New", chat_id, sender_id, now);
        auto m_other = create_test_message("Other", chat_id, other_sender_id, now - 5s);

        run_async(ioc, repo.addMessage(MessageDAOConverter::convert(m1)));
        run_async(ioc, repo.addMessage(MessageDAOConverter::convert(m2)));
        run_async(ioc, repo.addMessage(MessageDAOConverter::convert(m_other)));

        auto result = run_async(ioc, repo.getMessagesByChatAndSender(chat_id, sender_id));
        REQUIRE(result.size() == 2);
        CHECK(result[0].getMessageId() == m2.getMessageId());
        CHECK(result[1].getMessageId() == m1.getMessageId());
    }

    SUBCASE("getMessagesBySenderInChat returns empty list when sender has no messages in chat") {
        auto new_sender_id = uuid_gen();
        ChatMemberDAO new_sender(new_sender_id, chat_id, "new_sender", std::nullopt);
        storage->replace(new_sender);
        auto result = run_async(ioc, repo.getMessagesByChatAndSender(chat_id, new_sender_id));
        CHECK(result.empty());
    }

    SUBCASE("getMessagesByChatIdFromDate returns messages from given date onwards sorted") {
        using namespace std::chrono_literals;
        auto base = std::chrono::system_clock::now();
        auto m1 = create_test_message("Before", chat_id, sender_id, base - 1h);
        auto m2 = create_test_message("After", chat_id, sender_id, base);
        auto m3 = create_test_message("Later", chat_id, sender_id, base + 1h);

        run_async(ioc, repo.addMessage(MessageDAOConverter::convert(m1)));
        run_async(ioc, repo.addMessage(MessageDAOConverter::convert(m2)));
        run_async(ioc, repo.addMessage(MessageDAOConverter::convert(m3)));

        auto result = run_async(ioc, repo.getMessagesByChatFromDate(chat_id, base));
        REQUIRE(result.size() == 2);
        CHECK(result[0].getMessageId() == m3.getMessageId());
        CHECK(result[1].getMessageId() == m2.getMessageId());
    }

    SUBCASE("getMessagesByChatIdFromDate returns empty list when no messages after date") {
        auto future = std::chrono::system_clock::now() + std::chrono::hours(24);
        auto result = run_async(ioc, repo.getMessagesByChatFromDate(chat_id, future));
        CHECK(result.empty());
    }

    SUBCASE("getMessagesBySender returns all messages from sender across all chats sorted") {
        using namespace std::chrono_literals;
        auto now = std::chrono::system_clock::now();
        auto other_chat_id = uuid_gen();
        ChatDAO other_chat(other_chat_id, uuid_gen(), ChatType::Direct, "Other",
                           std::chrono::system_clock::now(), 0);
        storage->replace(other_chat);

        auto m1 = create_test_message("Chat1", chat_id, sender_id, now - 20s);
        auto m2 = create_test_message("Chat2", other_chat_id, sender_id, now);
        auto m_other = create_test_message("Other sender", chat_id, uuid_gen(), now - 5s);

        run_async(ioc, repo.addMessage(MessageDAOConverter::convert(m1)));
        run_async(ioc, repo.addMessage(MessageDAOConverter::convert(m2)));
        run_async(ioc, repo.addMessage(MessageDAOConverter::convert(m_other)));

        auto result = run_async(ioc, repo.getMessagesBySender(sender_id));
        REQUIRE(result.size() == 2);
        CHECK(result[0].getMessageId() == m2.getMessageId());
        CHECK(result[1].getMessageId() == m1.getMessageId());
    }

    SUBCASE("getMessagesBySender returns empty list when sender has no messages") {
        auto new_sender = uuid_gen();
        ChatMemberDAO member(new_sender, chat_id, "nosender", std::nullopt);
        storage->replace(member);
        auto result = run_async(ioc, repo.getMessagesBySender(new_sender));
        CHECK(result.empty());
    }

    SUBCASE("getMessagesBySenderInChatFromDate returns filtered and sorted messages") {
        using namespace std::chrono_literals;
        auto base = std::chrono::system_clock::now();
        auto m1 = create_test_message("Old", chat_id, sender_id, base - 1h);
        auto m2 = create_test_message("New1", chat_id, sender_id, base);
        auto m3 = create_test_message("New2", chat_id, sender_id, base + 30min);
        auto m_other_sender = create_test_message("Other", chat_id, uuid_gen(), base + 10min);
        auto m_other_chat = create_test_message("Other chat", uuid_gen(), sender_id, base + 20min);

        run_async(ioc, repo.addMessage(MessageDAOConverter::convert(m1)));
        run_async(ioc, repo.addMessage(MessageDAOConverter::convert(m2)));
        run_async(ioc, repo.addMessage(MessageDAOConverter::convert(m3)));
        run_async(ioc, repo.addMessage(MessageDAOConverter::convert(m_other_sender)));
        run_async(ioc, repo.addMessage(MessageDAOConverter::convert(m_other_chat)));

        auto result = run_async(ioc, repo.getMessagesByChatSenderFromDate(chat_id, sender_id, base));
        REQUIRE(result.size() == 2);
        CHECK(result[0].getMessageId() == m3.getMessageId());
        CHECK(result[1].getMessageId() == m2.getMessageId());
    }

    SUBCASE("getMessagesBySenderInChatFromDate returns empty list when no matching messages") {
        auto future = std::chrono::system_clock::now() + std::chrono::hours(24);
        auto result = run_async(ioc, repo.getMessagesByChatSenderFromDate(chat_id, sender_id, future));
        CHECK(result.empty());
    }
}
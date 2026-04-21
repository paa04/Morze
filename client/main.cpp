#include <iostream>
#include <memory>
#include <chrono>
#include <boost/uuid/uuid_generators.hpp>

#include "config/ConfigManager.h"
#include "repositories/dbContext/Database.h"
#include "repositories/DAO/ChatDAO.h"
#include "repositories/DAO/MessageDAO.h"
#include "globalConverters/UUIDConverter.h"

using namespace std::chrono_literals;

int main(int argc, char* argv[]) {
    try {
        // 1. Загружаем конфигурацию
        std::string configPath = "config.json";
        if (argc > 1) {
            configPath = argv[1];
        }
        ConfigManager config = ConfigManager::loadFromFile(configPath);
        std::cout << "Configuration loaded:\n"
                  << "  Database path: " << config.database().path << "\n"
                  << "  Server URL:    " << config.signaling().server_url << "\n"
                  << "  Log level:     " << config.logging().level << "\n";

        // 2. Инициализируем базу данных
        Database db(config.database().path);
        if (!db.init()) {
            std::cerr << "Failed to initialize database.\n";
            return 1;
        }
        std::cout << "Database initialized successfully.\n";

        auto storage = db.storage();

        // 3. Создаём тестовый чат (если его ещё нет)
        // Генерируем UUID для чата
        static boost::uuids::random_generator uuid_gen;
        boost::uuids::uuid chat_id = uuid_gen();
        boost::uuids::uuid room_id = uuid_gen(); // в реальности room_id приходит с сервера

        // Проверяем, есть ли уже такой чат (для демонстрации просто вставим)
        ChatDAO new_chat(
            chat_id,
            room_id,
            ChatType::Direct,
            "Test Chat",
            std::chrono::system_clock::now(),
            0
        );
        storage->replace(new_chat);
        std::cout << "Inserted test chat with ID: " << UUIDConverter::toString(chat_id) << "\n";

        // 4. Читаем все чаты и выводим
        auto all_chats = storage->get_all<ChatDAO>();
        std::cout << "\nAll chats (" << all_chats.size() << "):\n";
        for (const auto& c : all_chats) {
            std::cout << "  - " << c.getTitle()
                      << " (type: " << c.getTypeAsString()
                      << ", created: " << c.getCreatedAtAsString() << ")\n";
        }

        // 5. Тестовое сообщение
        boost::uuids::uuid msg_id = uuid_gen();
        MessageDAO new_msg(
            msg_id,
            chat_id,
            uuid_gen(),
            MessageDirection::Outgoing,
            "Hello, world!",
            std::chrono::system_clock::now(),
            DeliveryState::Sent
        );
        storage->replace(new_msg);

        auto blob_chat_id = UUIDConverter::toBlob(chat_id);
        auto messages = storage->get_all<MessageDAO>(sqlite_orm::where(sqlite_orm::c(&MessageDAO::getChatIdAsBLOB) == blob_chat_id));
        std::cout << "\nMessages in chat:\n";
        for (const auto& m : messages) {
            std::cout << "  [" << m.getCreatedAtAsString() << "] "
                      << m.getSenderIdAsString() << ": " << m.getContent()
                      << " (" << m.getDeliveryStateAsString() << ")\n";
        }

        // 6. Проверяем connection_profiles (пока пусто)
        auto profiles = storage->get_all<ConnectionProfileDAO>();
        std::cout << "\nConnection profiles: " << profiles.size() << "\n";

        std::cout << "\nAll tests passed.\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
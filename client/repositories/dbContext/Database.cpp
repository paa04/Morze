#include "Database.h"
#include <iostream>
#include <sqlite3.h>

Database::Database(const std::string &dbPath)
    : dbPath_(dbPath) {
    storage_ = std::make_shared<StorageType>(DBConfiguration::createStorage(dbPath));
}

Database::~Database() {
    if (raw_db_) {
        sqlite3_close(raw_db_);
    }
}

bool Database::execRaw(const std::string &sql) {
    char *errMsg = nullptr;
    int rc = sqlite3_exec(raw_db_, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << (errMsg ? errMsg : "unknown") << std::endl;
        if (errMsg) sqlite3_free(errMsg);
        return false;
    }
    return true;
}

int Database::querySingleInt(const std::string &sql) {
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(raw_db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(raw_db_) << std::endl;
        return -1;
    }
    int result = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return result;
}

bool Database::init() {
    int rc = sqlite3_open(dbPath_.c_str(), &raw_db_);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to open database: " << sqlite3_errmsg(raw_db_) << std::endl;
        return false;
    }

    if (!execRaw("CREATE TABLE IF NOT EXISTS schema_version (version INTEGER PRIMARY KEY);"))
        return false;

    if (querySingleInt("SELECT COUNT(*) FROM schema_version") == 0) {
        if (!execRaw("INSERT INTO schema_version (version) VALUES (0);"))
            return false;
    }

    return runMigrations();
}

int Database::getCurrentSchemaVersion() {
    return querySingleInt("SELECT version FROM schema_version LIMIT 1");
}

void Database::setSchemaVersion(int version) {
    execRaw("UPDATE schema_version SET version = " + std::to_string(version));
}

bool Database::applyMigration(int targetVersion, const std::string &sql) {
    try {
        if (!execRaw(sql))
            return false;
        setSchemaVersion(targetVersion);
        return true;
    } catch (const std::exception &e) {
        std::cerr << "Migration to version " << targetVersion << " failed: " << e.what() << std::endl;
        return false;
    }
}

bool Database::runMigrations() {
    int currentVersion = getCurrentSchemaVersion();
    const int TARGET_VERSION = 7; // увеличили, так как добавилась таблица chat_member_relation
    if (currentVersion >= TARGET_VERSION) {
        return true;
    }

    storage_->begin_transaction();

    // 001: chats
    if (currentVersion < 1) {
        std::string sql = R"(
            CREATE TABLE IF NOT EXISTS chats (
                id BLOB PRIMARY KEY,
                room_id BLOB NOT NULL,
                type TEXT NOT NULL CHECK(type IN ('direct','group')),
                title TEXT NOT NULL,
                created_at INTEGER NOT NULL,
                last_message_number INTEGER NOT NULL
            );
            CREATE INDEX IF NOT EXISTS idx_chats_room_id ON chats(room_id);
        )";
        if (!applyMigration(1, sql)) { storage_->rollback(); return false; }
    }

    // 002: chat_members (теперь с суррогатным id)
    if (currentVersion < 2) {
        std::string sql = R"(
            CREATE TABLE IF NOT EXISTS chat_members (
                id BLOB PRIMARY KEY,
                username TEXT NOT NULL,
                last_online_at INTEGER NULL
            );
        )";
        if (!applyMigration(2, sql)) { storage_->rollback(); return false; }
    }

    // 003: chat_member_relation (связь многие-ко-многим)
    if (currentVersion < 3) {
        std::string sql = R"(
            CREATE TABLE IF NOT EXISTS chat_member_relation (
                id BLOB PRIMARY KEY,
                chat_id BLOB NOT NULL,
                member_id BLOB NOT NULL,
                FOREIGN KEY(chat_id) REFERENCES chats(id) ON DELETE CASCADE,
                FOREIGN KEY(member_id) REFERENCES chat_members(id) ON DELETE CASCADE
            );
            CREATE INDEX IF NOT EXISTS idx_relation_chat_id ON chat_member_relation(chat_id);
            CREATE INDEX IF NOT EXISTS idx_relation_member_id ON chat_member_relation(member_id);
        )";
        if (!applyMigration(3, sql)) { storage_->rollback(); return false; }
    }

    // 004: messages
    if (currentVersion < 4) {
        std::string sql = R"(
            CREATE TABLE IF NOT EXISTS messages (
                id BLOB PRIMARY KEY,
                chat_id BLOB NOT NULL,
                sender_id BLOB NOT NULL,
                direction TEXT NOT NULL CHECK(direction IN ('incoming','outgoing')),
                content TEXT NOT NULL,
                created_at INTEGER NOT NULL,
                delivery_state TEXT NOT NULL,
                FOREIGN KEY(chat_id) REFERENCES chats(id) ON DELETE CASCADE,
                FOREIGN KEY(sender_id) REFERENCES chat_members(id) ON DELETE CASCADE
            );
            CREATE INDEX IF NOT EXISTS idx_messages_chat_id ON messages(chat_id);
            CREATE INDEX IF NOT EXISTS idx_messages_sender_id ON messages(sender_id);
            CREATE INDEX IF NOT EXISTS idx_messages_created_at ON messages(created_at);
        )";
        if (!applyMigration(4, sql)) { storage_->rollback(); return false; }
    }

    // 005: connection_profiles
    if (currentVersion < 5) {
        std::string sql = R"(
            CREATE TABLE IF NOT EXISTS connection_profiles (
                id BLOB PRIMARY KEY,
                server_url TEXT NOT NULL,
                stun_url TEXT NOT NULL,
                updated_at INTEGER NOT NULL
            );
        )";
        if (!applyMigration(5, sql)) { storage_->rollback(); return false; }
    }

    // 006: users
    if (currentVersion < 6) {
        std::string sql = R"(
            CREATE TABLE IF NOT EXISTS users (
                login TEXT PRIMARY KEY,
                password_hash TEXT NOT NULL,
                member_id BLOB NOT NULL UNIQUE,
                created_at INTEGER NOT NULL,
                updated_at INTEGER NOT NULL
            );
            CREATE INDEX IF NOT EXISTS idx_users_member_id ON users(member_id);
        )";
        if (!applyMigration(6, sql)) { storage_->rollback(); return false; }
    }

    // 007: финальная версия
    if (currentVersion < 7) {
        setSchemaVersion(7);
    }

    storage_->commit();
    return true;
}
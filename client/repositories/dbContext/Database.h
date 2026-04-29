#ifndef MORZE_DATABASE_H
#define MORZE_DATABASE_H

#include<memory>
#include<sqlite_orm/sqlite_orm.h>
#include <string>

#include "DBConfiguration.h"

struct sqlite3;  // forward declaration

class Database {
public:
    explicit Database(const std::string& dbPath);
    ~Database();

    bool init();
    auto storage() { return storage_; }

private:
    using StorageType = decltype(DBConfiguration::createStorage(""));
    std::shared_ptr<StorageType> storage_;
    std::string dbPath_;
    sqlite3* raw_db_ = nullptr;   // сырое соединение для миграций

    bool execRaw(const std::string& sql);
    int querySingleInt(const std::string& sql);
    int getCurrentSchemaVersion();
    void setSchemaVersion(int version);
    bool applyMigration(int targetVersion, const std::string& sql);
    bool runMigrations();
};


#endif //MORZE_DATABASE_H
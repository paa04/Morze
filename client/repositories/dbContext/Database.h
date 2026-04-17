#ifndef MORZE_DATABASE_H
#define MORZE_DATABASE_H

#include<memory>
#include<sqlite_orm/sqlite_orm.h>

#include<DBConfiguration.h>

class Database {
public:
    explicit Database(const std::string& dbPath);
    ~Database();

    // Инициализация: создание schema_version и запуск миграций
    bool init();

    // Доступ к хранилищу sqlite_orm
    auto storage() { return storage_; }

private:
    using StorageType = decltype(DBConfiguration::createStorage(""));
    std::shared_ptr<StorageType> storage_;

    int getCurrentSchemaVersion();
    void setSchemaVersion(int version);
    bool applyMigration(int targetVersion, const std::string& sql);
    bool runMigrations();
};


#endif //MORZE_DATABASE_H
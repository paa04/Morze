//
// Created by ivan on 18.04.2026.
//

#ifndef MORZE_CONFIGMANAGER_H
#define MORZE_CONFIGMANAGER_H
#include <string>
#include <nlohmann/json_fwd.hpp>


class ConfigManager {
public:
    struct Database {
        std::string path = "chat.db";
    };

    struct Logging {
        std::string level = "info";
        std::string file; // пустой – только консоль
    };

    struct Signaling {
        std::string server_url = "ws://localhost:9001";
        std::string canary_server_url;
        std::vector<std::string> stun_urls = {"stun:stun.l.google.com:19302"};
    };

    static ConfigManager loadFromFile(const std::string& filepath);

    const Database& database() const { return database_; }
    const Logging& logging() const { return logging_; }
    const Signaling& signaling() const { return signaling_; }

    void setDatabasePath(const std::string& path) { database_.path = path; }

private:
    Database database_;
    Logging logging_;
    Signaling signaling_;

    void parse(const nlohmann::json& j);
};


#endif //MORZE_CONFIGMANAGER_H
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "ConfigManager.h"

using json = nlohmann::json;

ConfigManager ConfigManager::loadFromFile(const std::string& filepath) {
    ConfigManager cfg;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Warning: config file not found, using defaults\n";
        return cfg;
    }

    try {
        json j = json::parse(file);
        cfg.parse(j);
    } catch (const json::parse_error& e) {
        std::cerr << "Failed to parse config: " << e.what() << ", using defaults\n";
    }
    return cfg;
}

void ConfigManager::parse(const json& j) {
    if (j.contains("database")) {
        auto& db = j["database"];
        if (db.contains("path"))
            database_.path = db["path"].get<std::string>();
    }

    if (j.contains("logging")) {
        auto& log = j["logging"];
        if (log.contains("level"))
            logging_.level = log["level"].get<std::string>();
        if (log.contains("file"))
            logging_.file = log["file"].get<std::string>();
    }

    if (j.contains("signaling")) {
        auto& sig = j["signaling"];
        if (sig.contains("server_url"))
            signaling_.server_url = sig["server_url"].get<std::string>();
    }
}
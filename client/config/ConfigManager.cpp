#include <fstream>
#include <iostream>
#include <boost/json.hpp>

#include "ConfigManager.h"

namespace json = boost::json;

ConfigManager ConfigManager::loadFromFile(const std::string& filepath) {
    ConfigManager cfg;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Warning: config file not found, using defaults\n";
        return cfg;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    boost::system::error_code ec;
    json::value jv = json::parse(content, ec);
    if (ec) {
        std::cerr << "Failed to parse config: " << ec.message() << ", using defaults\n";
        return cfg;
    }

    if (auto obj = jv.if_object()) {
        cfg.parse(*obj);
    }
    return cfg;
}

void ConfigManager::parse(const json::object& obj) {
    if (auto db = obj.if_contains("database")) {
        if (auto dbObj = db->if_object()) {
            if (auto path = dbObj->if_contains("path"))
                database_.path = json::value_to<std::string>(*path);
        }
    }

    if (auto log = obj.if_contains("logging")) {
        if (auto logObj = log->if_object()) {
            if (auto level = logObj->if_contains("level"))
                logging_.level = json::value_to<std::string>(*level);
            if (auto file = logObj->if_contains("file"))
                logging_.file = json::value_to<std::string>(*file);
        }
    }

    if (auto sig = obj.if_contains("signaling")) {
        if (auto sigObj = sig->if_object()) {
            if (auto url = sigObj->if_contains("server_url"))
                signaling_.server_url = json::value_to<std::string>(*url);
        }
    }
}
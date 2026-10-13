// ServerConfig.cpp: JSON parsing and validation for the server settings.
#include "config/ServerConfig.h"

#include <fstream>
#include <initializer_list>
#include <nlohmann/json.hpp>
#include <set>
#include <sstream>

namespace matchforge::config {

namespace {

using nlohmann::json;

// Rejects keys we do not know, so typos are caught instead of silently ignored.
void requireOnlyKeys(const json& object, const std::string& path,
                     std::initializer_list<const char*> allowed) {
    for (const auto& [key, value] : object.items()) {
        bool known = false;
        for (const char* name : allowed) {
            known = known || key == name;
        }
        if (!known) {
            throw ConfigError("unknown setting '" + path + key + "'");
        }
    }
}

const json* find(const json& object, const char* key) {
    const auto it = object.find(key);
    return it == object.end() ? nullptr : &*it;
}

// Reads an optional whole number in [min, max] into `out`. A missing key keeps the default.
template <class T>
void readInt(const json& object, const std::string& path, const char* key, long long min,
             long long max, T& out) {
    const json* value = find(object, key);
    if (value == nullptr) {
        return;
    }
    if (!value->is_number_integer()) {
        throw ConfigError("'" + path + key + "' must be a whole number");
    }
    const long long number = value->get<long long>();
    if (number < min || number > max) {
        throw ConfigError("'" + path + key + "' must be between " + std::to_string(min) + " and " +
                          std::to_string(max));
    }
    out = static_cast<T>(number);
}

void readDouble(const json& object, const std::string& path, const char* key, double min,
                double max, double& out) {
    const json* value = find(object, key);
    if (value == nullptr) {
        return;
    }
    if (!value->is_number()) {
        throw ConfigError("'" + path + key + "' must be a number");
    }
    const double number = value->get<double>();
    if (number < min || number > max) {
        std::ostringstream message;
        message << "'" << path << key << "' must be between " << min << " and " << max;
        throw ConfigError(message.str());
    }
    out = number;
}

void readString(const json& object, const std::string& path, const char* key, std::string& out) {
    const json* value = find(object, key);
    if (value == nullptr) {
        return;
    }
    if (!value->is_string() || value->get<std::string>().empty()) {
        throw ConfigError("'" + path + key + "' must be a non-empty string");
    }
    out = value->get<std::string>();
}

// Mode and region names must be short and plain: 1 to 20 letters, digits, '_' or '-'.
// That keeps them safe to log and to use as metric labels. (Usernames have a 3-character
// minimum; region codes such as "eu" are shorter, so they get their own rule.)
void requireSafeName(const std::string& name, const std::string& what) {
    bool valid = !name.empty() && name.size() <= 20;
    for (const char c : name) {
        valid = valid && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                          (c >= '0' && c <= '9') || c == '_' || c == '-');
    }
    if (!valid) {
        throw ConfigError(what + " '" + name + "' must be 1 to 20 letters, digits, '_' or '-'");
    }
}

}  // namespace

void validate(const ServerConfig& config) {
    if (config.matchSettings.baseRatingWindow > config.matchSettings.maxRatingWindow) {
        throw ConfigError(
            "'matchmaking.baseRatingWindow' must not be larger than 'matchmaking.maxRatingWindow'");
    }
    if (config.messageBurst < config.messagesPerSecond) {
        throw ConfigError("'limits.messageBurst' must be at least 'limits.messagesPerSecond'");
    }
    if (config.metricsPort != 0 && config.metricsPort == config.port) {
        throw ConfigError("'metricsPort' must differ from 'port'");
    }
    if (config.regions.empty()) {
        throw ConfigError("'matchmaking.regions' must list at least one region");
    }
    if (config.modes.empty()) {
        throw ConfigError("'matchmaking.modes' must list at least one mode");
    }
    std::set<std::string> regions;
    for (const std::string& region : config.regions) {
        requireSafeName(region, "region");
        if (!regions.insert(region).second) {
            throw ConfigError("region '" + region + "' is listed twice");
        }
    }
    std::set<std::string> modes;
    for (const core::MatchMode& mode : config.modes) {
        requireSafeName(mode.name, "mode");
        if (mode.teamSize < 1 || mode.teamSize > 16) {
            throw ConfigError("mode '" + mode.name + "' teamSize must be between 1 and 16");
        }
        if (!modes.insert(mode.name).second) {
            throw ConfigError("mode '" + mode.name + "' is listed twice");
        }
    }
}

ServerConfig parseConfig(const std::string& jsonText) {
    json root;
    try {
        root = json::parse(jsonText);
    } catch (const json::parse_error& error) {
        throw ConfigError(std::string("config is not valid JSON: ") + error.what());
    }
    if (!root.is_object()) {
        throw ConfigError("config must be a JSON object");
    }

    ServerConfig config;
    requireOnlyKeys(root, "",
                    {"port", "maxFrameBytes", "metricsPort", "workerThreads", "redis",
                     "matchmaking", "limits"});
    readInt(root, "", "port", 1, 65535, config.port);
    readInt(root, "", "maxFrameBytes", 1024, 16 * 1024 * 1024, config.maxFrameBytes);
    readInt(root, "", "metricsPort", 0, 65535, config.metricsPort);
    readInt(root, "", "workerThreads", 1, 64, config.workerThreads);

    if (const json* redis = find(root, "redis")) {
        if (!redis->is_object()) {
            throw ConfigError("'redis' must be an object");
        }
        requireOnlyKeys(*redis, "redis.", {"host", "port"});
        readString(*redis, "redis.", "host", config.redisHost);
        int redisPort = std::stoi(config.redisPort);
        readInt(*redis, "redis.", "port", 1, 65535, redisPort);
        config.redisPort = std::to_string(redisPort);
    }

    if (const json* mm = find(root, "matchmaking")) {
        if (!mm->is_object()) {
            throw ConfigError("'matchmaking' must be an object");
        }
        const std::string path = "matchmaking.";
        requireOnlyKeys(*mm, path,
                        {"tickMilliseconds", "baseRatingWindow", "windowGrowthPerSecond",
                         "maxRatingWindow", "kFactor", "regions", "modes"});
        long long tick = config.tickInterval.count();
        readInt(*mm, path, "tickMilliseconds", 10, 5000, tick);
        config.tickInterval = std::chrono::milliseconds(tick);
        readInt(*mm, path, "baseRatingWindow", 0, 5000, config.matchSettings.baseRatingWindow);
        readInt(*mm, path, "windowGrowthPerSecond", 0, 1000,
                config.matchSettings.windowGrowthPerSecond);
        readInt(*mm, path, "maxRatingWindow", 0, 5000, config.matchSettings.maxRatingWindow);
        readInt(*mm, path, "kFactor", 1, 100, config.kFactor);

        if (const json* regions = find(*mm, "regions")) {
            if (!regions->is_array()) {
                throw ConfigError("'matchmaking.regions' must be a list of names");
            }
            config.regions.clear();
            for (const json& region : *regions) {
                if (!region.is_string()) {
                    throw ConfigError("'matchmaking.regions' must contain only strings");
                }
                config.regions.push_back(region.get<std::string>());
            }
        }
        if (const json* modes = find(*mm, "modes")) {
            if (!modes->is_array()) {
                throw ConfigError("'matchmaking.modes' must be a list of {name, teamSize}");
            }
            config.modes.clear();
            for (const json& mode : *modes) {
                if (!mode.is_object()) {
                    throw ConfigError("each entry in 'matchmaking.modes' must be an object");
                }
                requireOnlyKeys(mode, "matchmaking.modes[].", {"name", "teamSize"});
                core::MatchMode parsed;
                readString(mode, "matchmaking.modes[].", "name", parsed.name);
                readInt(mode, "matchmaking.modes[].", "teamSize", 1, 16, parsed.teamSize);
                config.modes.push_back(parsed);
            }
        }
    }

    if (const json* limits = find(root, "limits")) {
        if (!limits->is_object()) {
            throw ConfigError("'limits' must be an object");
        }
        const std::string path = "limits.";
        requireOnlyKeys(*limits, path,
                        {"idleTimeoutSeconds", "messagesPerSecond", "messageBurst",
                         "maxLoginFailures", "loginLockoutSeconds"});
        long long idle = config.idleTimeout.count();
        readInt(*limits, path, "idleTimeoutSeconds", 5, 3600, idle);
        config.idleTimeout = std::chrono::seconds(idle);
        readDouble(*limits, path, "messagesPerSecond", 0.1, 10000, config.messagesPerSecond);
        readDouble(*limits, path, "messageBurst", 1, 100000, config.messageBurst);
        readInt(*limits, path, "maxLoginFailures", 1, 1000, config.maxLoginFailures);
        long long lockout = config.loginLockout.count();
        readInt(*limits, path, "loginLockoutSeconds", 1, 86400, lockout);
        config.loginLockout = std::chrono::seconds(lockout);
    }

    validate(config);
    return config;
}

ServerConfig loadConfigFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw ConfigError("cannot open config file '" + path + "'");
    }
    std::stringstream contents;
    contents << file.rdbuf();
    return parseConfig(contents.str());
}

}  // namespace matchforge::config

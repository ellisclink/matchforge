// ServerConfig.h: every setting the server reads at startup, and the JSON loader for them.
//
// Settings come from a JSON file (config/server.json). Every field is optional: a missing
// field keeps the default below. Every value is checked (type and range), and unknown keys
// are errors, so a typo like "kfactor" stops the server with a clear message instead of
// being silently ignored.
//
// Secrets (the match result secret) are never read from this file: files get committed and
// copied around. They come from environment variables instead.
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/Matchmaker.h"

namespace matchforge::config {

struct ServerConfig {
    // Network
    std::uint16_t port = 7777;
    std::size_t maxFrameBytes = 64 * 1024;  // largest message a client may send
    std::uint16_t metricsPort = 9100;       // Prometheus metrics endpoint; 0 turns it off

    // Threads that hash passwords and talk to Redis.
    unsigned int workerThreads = 4;

    // Redis. MATCHFORGE_REDIS_HOST and MATCHFORGE_REDIS_PORT override these.
    std::string redisHost = "127.0.0.1";
    std::string redisPort = "6379";

    // Matchmaking
    std::chrono::milliseconds tickInterval{250};
    core::MatchSettings matchSettings;  // rating window: base, growth per second, maximum
    int kFactor = 32;
    std::vector<std::string> regions{"eu", "na", "asia"};
    std::vector<core::MatchMode> modes{{"duel", 1}, {"duo", 2}, {"squad", 4}};

    // Abuse limits
    std::chrono::seconds idleTimeout{60};  // close connections that send nothing for this long
    double messagesPerSecond = 20;         // per connection, long-run average
    double messageBurst = 40;              // per connection, short bursts allowed
    int maxLoginFailures = 5;              // wrong passwords before a username is locked
    std::chrono::seconds loginLockout{300};
};

// Thrown for any problem with the configuration. The message says what and where.
class ConfigError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Parses JSON text into a config, starting from the defaults. Throws ConfigError.
ServerConfig parseConfig(const std::string& jsonText);

// Reads and parses a file. Throws ConfigError if it cannot be read or is invalid.
ServerConfig loadConfigFile(const std::string& path);

// Checks values that depend on each other (for example base window <= max window).
// parseConfig calls it; it is public so code that builds a config by hand can use it too.
void validate(const ServerConfig& config);

}  // namespace matchforge::config

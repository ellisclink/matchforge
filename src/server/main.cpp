// main.cpp for the server: the program's entry point. The operating system
// calls main() first when the program starts.
//
// Usage: matchforge_server [--config <file.json>] [--port <number>]
//   --config  settings file (see config/server.json); without it, built-in defaults are used
//   --port    overrides the port from the config file
// Environment:
//   MATCHFORGE_REDIS_HOST, MATCHFORGE_REDIS_PORT  override the Redis address from the config
//   MATCHFORGE_RESULT_SECRET                      the secret game servers send with results
#include <spdlog/spdlog.h>  // Logging library. Provides spdlog::info and friends.

#include <algorithm>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/thread_pool.hpp>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "auth/AuthService.h"
#include "config/ServerConfig.h"
#include "core/Version.h"
#include "lobby/LobbyService.h"
#include "net/Server.h"
#include "store/RedisAccountStore.h"

namespace {

// What the command line asked for. Anything not given keeps the config file's value.
struct CommandLine {
    std::optional<std::string> configPath;
    std::optional<std::uint16_t> port;
};

// Returns the parsed command line, or nullopt (after logging why) if it is invalid.
std::optional<CommandLine> parseCommandLine(int argc, char* argv[]) {
    CommandLine result;
    for (int i = 1; i < argc; ++i) {
        const std::string flag = argv[i];
        if (i + 1 >= argc) {
            spdlog::error("missing value after {}", flag);
            return std::nullopt;
        }
        const std::string value = argv[++i];
        if (flag == "--config") {
            result.configPath = value;
        } else if (flag == "--port") {
            char* end = nullptr;
            const unsigned long number = std::strtoul(value.c_str(), &end, 10);
            if (value.empty() || *end != '\0' || number < 1 || number > 65535) {
                spdlog::error("--port must be a number from 1 to 65535");
                return std::nullopt;
            }
            result.port = static_cast<std::uint16_t>(number);
        } else {
            spdlog::error("unknown option {} (usage: matchforge_server [--config file] [--port n])",
                          flag);
            return std::nullopt;
        }
    }
    return result;
}

// Reads an environment variable, or returns nullopt when it is not set.
std::optional<std::string> env(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::optional<std::string>(value) : std::nullopt;
}

}  // namespace

int main(int argc, char* argv[]) {
    spdlog::info("matchforge server {}", matchforge::core::version());

    const auto commandLine = parseCommandLine(argc, argv);
    if (!commandLine) {
        return 2;
    }

    // 1. Settings: config file (or defaults), then command line, then environment.
    matchforge::config::ServerConfig config;
    try {
        if (commandLine->configPath) {
            config = matchforge::config::loadConfigFile(*commandLine->configPath);
            spdlog::info("loaded config {}", *commandLine->configPath);
        } else {
            spdlog::info("no --config given, using built-in defaults");
        }
    } catch (const matchforge::config::ConfigError& error) {
        // A bad config must stop the server: running with half-understood settings is worse.
        spdlog::error("invalid config: {}", error.what());
        return 2;
    }
    if (commandLine->port) {
        config.port = *commandLine->port;
    }
    config.redisHost = env("MATCHFORGE_REDIS_HOST").value_or(config.redisHost);
    config.redisPort = env("MATCHFORGE_REDIS_PORT").value_or(config.redisPort);
    const std::string resultSecret = env("MATCHFORGE_RESULT_SECRET").value_or("");

    try {
        // 2. Storage first, so it outlives everything that uses it.
        matchforge::store::RedisAccountStore store(config.redisHost, config.redisPort);
        spdlog::info("accounts stored in redis at {}:{}", config.redisHost, config.redisPort);

        // 3. Worker threads for password hashing and Redis calls, away from the network threads.
        boost::asio::thread_pool workers(config.workerThreads);
        matchforge::core::LoginThrottleSettings throttle;
        throttle.maxFailures = config.maxLoginFailures;
        throttle.window = config.loginLockout;
        throttle.lockout = config.loginLockout;
        matchforge::auth::AuthService auth(store, workers, throttle);

        // 4. The event loop. It runs every network callback.
        boost::asio::io_context ioContext;

        // Ctrl+C (SIGINT) and docker stop (SIGTERM) stop the event loop cleanly,
        // instead of killing the process in the middle of a write.
        boost::asio::signal_set signals(ioContext, SIGINT, SIGTERM);
        signals.async_wait([&ioContext](boost::system::error_code, int signalNumber) {
            spdlog::info("received signal {}, shutting down", signalNumber);
            ioContext.stop();
        });

        // 5. The matchmaking queue.
        matchforge::lobby::LobbySettings lobbySettings;
        lobbySettings.modes = config.modes;
        lobbySettings.regions = config.regions;
        lobbySettings.matchSettings = config.matchSettings;
        lobbySettings.tickInterval = config.tickInterval;
        lobbySettings.kFactor = config.kFactor;
        lobbySettings.resultSecret = resultSecret;
        if (resultSecret.empty()) {
            spdlog::warn("MATCHFORGE_RESULT_SECRET is not set: match results will be rejected");
        }
        matchforge::lobby::LobbyService lobby(ioContext, store, workers, lobbySettings);
        lobby.start();

        // 6. The listener, with the per-connection limits from the config.
        matchforge::net::SessionLimits limits;
        limits.maxFrameBytes = config.maxFrameBytes;
        limits.idleTimeout = config.idleTimeout;
        limits.messagesPerSecond = config.messagesPerSecond;
        limits.messageBurst = config.messageBurst;
        matchforge::net::Server server(ioContext, config.port, limits, auth, lobby);
        server.start();
        spdlog::info("listening on port {} ({} modes, {} regions)", server.localPort(),
                     config.modes.size(), config.regions.size());

        // 7. Run the event loop on several threads, so one slow client does not block others.
        // Each session and the lobby have their own strand, so their handlers stay in order.
        const unsigned int threadCount = std::max(2u, std::thread::hardware_concurrency());
        std::vector<std::thread> ioThreads;
        ioThreads.reserve(threadCount);
        for (unsigned int i = 0; i < threadCount; ++i) {
            ioThreads.emplace_back([&ioContext] { ioContext.run(); });
        }
        for (auto& thread : ioThreads) {
            thread.join();  // Returns once ioContext.stop() has been called.
        }

        // Let work still running on the workers finish before the store is destroyed.
        workers.join();
        spdlog::info("server stopped");
    } catch (const std::exception& error) {
        // Most often: the port is already in use. Log it and exit with an error code.
        spdlog::error("server failed: {}", error.what());
        return 1;
    }

    // 0 tells the operating system (and Docker) the program finished successfully.
    return 0;
}

// main.cpp for the server: the program's entry point. The operating system
// calls main() first when the program starts.
#include <spdlog/spdlog.h>  // Logging library. Provides spdlog::info and friends.

#include <algorithm>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/thread_pool.hpp>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "auth/AuthService.h"
#include "core/Version.h"        // Gives us matchforge::core::version().
#include "lobby/LobbyService.h"  // The matchmaking queue.
#include "net/Server.h"          // The listening socket.
#include "store/RedisAccountStore.h"

namespace {

// Port used when the command line does not give one.
constexpr std::uint16_t kDefaultPort = 7777;

// Largest message a client may send, in bytes. A bigger length header closes the connection.
constexpr std::size_t kMaxFrameBytes = 64 * 1024;

// Threads that hash passwords. Argon2id is CPU- and memory-heavy, so this is kept small:
// each hash uses 64 MB, and this many run at once.
constexpr unsigned int kAuthWorkerThreads = 4;

// Reads the optional port argument: "matchforge_server 9000". Returns false if it is not a valid
// port.
bool parsePort(int argc, char* argv[], std::uint16_t& port) {
    port = kDefaultPort;
    if (argc < 2) {
        return true;  // No argument: use the default.
    }

    char* end = nullptr;
    const unsigned long value = std::strtoul(argv[1], &end, 10);
    if (end == argv[1] || *end != '\0' || value > 65535) {
        return false;  // Not a number, or outside the valid port range.
    }
    port = static_cast<std::uint16_t>(value);
    return true;
}

// Reads a setting from the environment, or returns the default when it is not set.
// Docker Compose passes Redis's address this way, so the same binary runs anywhere.
std::string envOr(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : fallback;
}

}  // namespace

int main(int argc, char* argv[]) {
    spdlog::info("matchforge server {}", matchforge::core::version());

    std::uint16_t port = kDefaultPort;
    if (!parsePort(argc, argv, port)) {
        spdlog::error("invalid port argument, expected a number from 0 to 65535");
        return 1;
    }

    try {
        // Storage first, so it outlives everything that uses it.
        const std::string redisHost = envOr("MATCHFORGE_REDIS_HOST", "127.0.0.1");
        const std::string redisPort = envOr("MATCHFORGE_REDIS_PORT", "6379");
        matchforge::store::RedisAccountStore store(redisHost, redisPort);
        spdlog::info("accounts stored in redis at {}:{}", redisHost, redisPort);

        // Password hashing runs here, away from the network threads.
        boost::asio::thread_pool authWorkers(kAuthWorkerThreads);
        matchforge::auth::AuthService auth(store, authWorkers);

        // The io_context is the event loop: it runs every network callback.
        boost::asio::io_context ioContext;

        // Ctrl+C (SIGINT) and docker stop (SIGTERM) stop the event loop cleanly,
        // instead of killing the process in the middle of a write.
        boost::asio::signal_set signals(ioContext, SIGINT, SIGTERM);
        signals.async_wait([&ioContext](boost::system::error_code, int signalNumber) {
            spdlog::info("received signal {}, shutting down", signalNumber);
            ioContext.stop();
        });

        // The matchmaking queue. Modes and regions are fixed here for now; Milestone 8 moves
        // them into a JSON config file.
        matchforge::lobby::LobbySettings lobbySettings;
        lobbySettings.modes = {{"duel", 1}, {"duo", 2}, {"squad", 4}};
        lobbySettings.regions = {"eu", "na", "asia"};
        // Game servers prove they may report results with this secret. Without it, results are
        // rejected, so ratings cannot be changed by anyone.
        lobbySettings.resultSecret = envOr("MATCHFORGE_RESULT_SECRET", "");
        if (lobbySettings.resultSecret.empty()) {
            spdlog::warn("MATCHFORGE_RESULT_SECRET is not set: match results will be rejected");
        }
        matchforge::lobby::LobbyService lobby(ioContext, store, authWorkers, lobbySettings);
        lobby.start();

        matchforge::net::Server server(ioContext, port, kMaxFrameBytes, auth, lobby);
        server.start();
        spdlog::info("listening on port {}", server.localPort());

        // Run the event loop on several threads, so one slow client does not block others.
        // Each session has its own strand, so its handlers stay in order even with many threads.
        const unsigned int threadCount = std::max(2u, std::thread::hardware_concurrency());
        std::vector<std::thread> ioThreads;
        ioThreads.reserve(threadCount);
        for (unsigned int i = 0; i < threadCount; ++i) {
            ioThreads.emplace_back([&ioContext] { ioContext.run(); });
        }
        for (auto& thread : ioThreads) {
            thread.join();  // Returns once ioContext.stop() has been called and all work is done.
        }

        // Let any password hash that is still running finish before the store is destroyed.
        authWorkers.join();
        spdlog::info("server stopped");
    } catch (const std::exception& error) {
        // Most often: the port is already in use. Log it and exit with an error code.
        spdlog::error("server failed: {}", error.what());
        return 1;
    }

    // 0 tells the operating system (and Docker) the program finished successfully.
    return 0;
}

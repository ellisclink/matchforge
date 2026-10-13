// main.cpp for the server: the program's entry point. The operating system
// calls main() first when the program starts.
#include <spdlog/spdlog.h>  // Logging library. Provides spdlog::info and friends.

#include <algorithm>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <string>
#include <thread>
#include <vector>

#include "core/Version.h"  // Gives us matchforge::core::version().
#include "net/Server.h"    // The listening socket.

namespace {

// Port used when the command line does not give one.
constexpr std::uint16_t kDefaultPort = 7777;

// Largest message a client may send, in bytes. A bigger length header closes the connection.
constexpr std::size_t kMaxFrameBytes = 64 * 1024;

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

}  // namespace

int main(int argc, char* argv[]) {
    spdlog::info("matchforge server {}", matchforge::core::version());

    std::uint16_t port = kDefaultPort;
    if (!parsePort(argc, argv, port)) {
        spdlog::error("invalid port argument, expected a number from 0 to 65535");
        return 1;
    }

    try {
        // The io_context is the event loop: it runs every network callback.
        boost::asio::io_context ioContext;

        // Ctrl+C (SIGINT) and docker stop (SIGTERM) stop the event loop cleanly,
        // instead of killing the process in the middle of a write.
        boost::asio::signal_set signals(ioContext, SIGINT, SIGTERM);
        signals.async_wait([&ioContext](boost::system::error_code, int signalNumber) {
            spdlog::info("received signal {}, shutting down", signalNumber);
            ioContext.stop();
        });

        matchforge::net::Server server(ioContext, port, kMaxFrameBytes);
        server.start();
        spdlog::info("listening on port {}", server.localPort());

        // Run the event loop on several threads, so one slow client does not block others.
        // Each session has its own strand, so its handlers stay in order even with many threads.
        const unsigned int threadCount = std::max(2u, std::thread::hardware_concurrency());
        std::vector<std::thread> workers;
        workers.reserve(threadCount);
        for (unsigned int i = 0; i < threadCount; ++i) {
            workers.emplace_back([&ioContext] { ioContext.run(); });
        }
        for (auto& worker : workers) {
            worker.join();  // Returns once ioContext.stop() has been called and all work is done.
        }

        spdlog::info("server stopped");
    } catch (const std::exception& error) {
        // Most often: the port is already in use. Log it and exit with an error code.
        spdlog::error("server failed: {}", error.what());
        return 1;
    }

    // 0 tells the operating system (and Docker) the program finished successfully.
    return 0;
}

// RedisAccountStore.h: an AccountStore that keeps accounts and sessions in Redis.
#pragma once

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/redis/connection.hpp>
#include <boost/redis/request.hpp>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include "store/AccountStore.h"

namespace matchforge::store {

// Redis is reached through Boost.Redis, which is asynchronous. This class hides that:
// it runs its own I/O thread for the connection, and each call waits for its command to finish.
//
// Callers must not be I/O threads, because the call blocks. The auth worker threads make
// these calls, which keeps the network threads free (CLAUDE.md: never block an I/O thread).
class RedisAccountStore : public AccountStore {
public:
    // Connects lazily: the first command opens the connection. Throws nothing; failures
    // show up as std::runtime_error from the methods below.
    RedisAccountStore(std::string host, std::string port);

    // Closes the connection and stops the I/O thread.
    ~RedisAccountStore() override;

    RedisAccountStore(const RedisAccountStore&) = delete;
    RedisAccountStore& operator=(const RedisAccountStore&) = delete;

    bool createAccount(std::string_view username, std::string_view passwordHash) override;
    std::optional<std::string> findPasswordHash(std::string_view username) override;
    void saveSession(std::string_view token, std::string_view username,
                     std::chrono::seconds ttl) override;
    std::optional<std::string> findSessionUser(std::string_view token) override;

private:
    // Sends one request on the I/O thread and waits for it to finish. The response object
    // must outlive the call; it is filled in before this returns.
    template <class Response>
    void run(boost::redis::request& request, Response& response);

    boost::asio::io_context m_io;  // Event loop for the Redis connection.
    // Keeps m_io running even when no operation is pending, so posted commands always run.
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> m_guard;
    boost::redis::connection m_connection;
    std::thread m_thread;  // Runs m_io.
};

}  // namespace matchforge::store

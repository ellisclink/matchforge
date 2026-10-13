// RedisAccountStore.cpp: Redis commands for accounts and sessions.
#include "store/RedisAccountStore.h"

#include <boost/asio/detached.hpp>
#include <boost/asio/post.hpp>
#include <boost/redis/config.hpp>
#include <boost/redis/response.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

namespace matchforge::store {

namespace {

// How long a single Redis command may take before we report an error. Redis answers in
// well under a millisecond on a healthy network, so this only fires when Redis is down.
constexpr std::chrono::seconds kCommandTimeout{2};

// Keys are namespaced so accounts and sessions cannot collide in one Redis database.
std::string accountKey(std::string_view username) { return "account:" + std::string(username); }

std::string sessionKey(std::string_view token) { return "session:" + std::string(token); }

}  // namespace

RedisAccountStore::RedisAccountStore(std::string host, std::string port)
    : m_guard(boost::asio::make_work_guard(m_io)), m_connection(m_io.get_executor()) {
    boost::redis::config config;
    config.addr.host = std::move(host);
    config.addr.port = std::move(port);

    // Starts the connection loop. It runs on m_io's thread once that thread starts below.
    // async_run reconnects on its own if Redis drops the link.
    m_connection.async_run(config, boost::asio::detached);

    m_thread = std::thread([this] { m_io.run(); });
}

RedisAccountStore::~RedisAccountStore() {
    m_connection.cancel();  // Ends the connection loop.
    m_guard.reset();        // Lets m_io return once nothing is pending.
    m_io.stop();
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

template <class Response>
void RedisAccountStore::run(boost::redis::request& request, Response& response) {
    // Everything the I/O thread touches lives in this shared object, not on our stack.
    // If we give up on a slow command, the I/O thread may still finish it later, and it
    // must not write into memory that has already been freed.
    struct Pending {
        boost::redis::request request;
        Response response;
        std::promise<boost::system::error_code> finished;
    };
    auto pending = std::make_shared<Pending>();
    pending->request = request;
    std::future<boost::system::error_code> outcome = pending->finished.get_future();

    // Boost.Redis objects are not thread-safe, so the command is posted to the I/O thread.
    boost::asio::post(m_io, [this, pending] {
        m_connection.async_exec(pending->request, pending->response,
                                [pending](boost::system::error_code ec, std::size_t) {
                                    pending->finished.set_value(ec);
                                });
    });

    // Boost.Redis keeps retrying while Redis is down, so a command sent then never finishes
    // on its own. The timeout turns that into an error the caller can report.
    if (outcome.wait_for(kCommandTimeout) != std::future_status::ready) {
        throw std::runtime_error("redis command timed out (is redis running?)");
    }

    const boost::system::error_code ec = outcome.get();
    if (ec) {
        // Connection-level failure: Redis is down, or the command could not be sent.
        throw std::runtime_error("redis command failed: " + ec.message());
    }
    response = std::move(pending->response);
}

bool RedisAccountStore::createAccount(std::string_view username, std::string_view passwordHash) {
    // SETNX sets the key only if it does not exist yet. That makes "check the name is free"
    // and "claim the name" one atomic step, so two registrations cannot both succeed.
    boost::redis::request request;
    request.push("SETNX", accountKey(username), std::string(passwordHash));

    boost::redis::response<long long> response;
    run(request, response);

    const auto& result = std::get<0>(response);
    if (result.has_error()) {
        throw std::runtime_error("redis SETNX failed");
    }
    return result.value() == 1;  // 1 means the key was set, 0 means it already existed.
}

std::optional<std::string> RedisAccountStore::findPasswordHash(std::string_view username) {
    boost::redis::request request;
    request.push("GET", accountKey(username));

    boost::redis::response<std::optional<std::string>> response;
    run(request, response);

    const auto& result = std::get<0>(response);
    if (result.has_error()) {
        throw std::runtime_error("redis GET failed");
    }
    return result.value();  // Empty optional when the key does not exist.
}

void RedisAccountStore::saveSession(std::string_view token, std::string_view username,
                                    std::chrono::seconds ttl) {
    // SET with EX stores the value and sets an expiry in one command, so a session cannot
    // be left behind forever.
    boost::redis::request request;
    request.push("SET", sessionKey(token), std::string(username), "EX", ttl.count());

    boost::redis::response<std::string> response;
    run(request, response);

    if (std::get<0>(response).has_error()) {
        throw std::runtime_error("redis SET failed");
    }
}

std::optional<std::string> RedisAccountStore::findSessionUser(std::string_view token) {
    boost::redis::request request;
    request.push("GET", sessionKey(token));

    boost::redis::response<std::optional<std::string>> response;
    run(request, response);

    const auto& result = std::get<0>(response);
    if (result.has_error()) {
        throw std::runtime_error("redis GET failed");
    }
    return result.value();
}

}  // namespace matchforge::store

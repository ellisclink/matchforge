// AuthService.cpp: the register and login rules. Runs on worker threads.
#include "auth/AuthService.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <boost/asio/post.hpp>
#include <cctype>
#include <chrono>
#include <exception>
#include <optional>
#include <string>
#include <utility>

#include "auth/PasswordHasher.h"
#include "core/Validation.h"

namespace matchforge::auth {

namespace {

// Error codes, the same numbers as the ErrorResponse codes the server sends.
constexpr std::int32_t kBadRequest = 400;
constexpr std::int32_t kInvalidCredentials = 401;
constexpr std::int32_t kUsernameTaken = 409;
constexpr std::int32_t kServiceUnavailable = 503;

// How long a login stays valid. After this the client must log in again.
constexpr std::chrono::seconds kSessionLifetime = std::chrono::hours(24);

// Usernames are stored in lowercase, so "Alice" and "alice" are the same account.
std::string toLowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

// A hash that never matches a real password. Login checks the password even when the
// account does not exist, so an attacker cannot tell existing usernames from unknown ones by
// timing.
const std::string& dummyHash() {
    static const std::string hash = hashPassword("matchforge-dummy-password");
    return hash;
}

AuthResult failure(std::int32_t code, std::string message) {
    AuthResult result;
    result.ok = false;
    result.errorCode = code;
    result.errorMessage = std::move(message);
    return result;
}

AuthResult doRegister(store::AccountStore& store, const std::string& rawUsername,
                      const std::string& password) {
    // Check the input before doing any expensive work.
    if (!core::isValidUsername(rawUsername)) {
        return failure(kBadRequest, "username must be 3 to 20 letters, digits or underscores");
    }
    if (!core::isValidPassword(password)) {
        return failure(kBadRequest, "password must be 8 to 128 characters");
    }

    const std::string username = toLowercase(rawUsername);
    try {
        // Hashing takes about 50 to 100 ms. It happens only after the input is valid.
        const std::string hash = hashPassword(password);
        if (!store.createAccount(username, hash)) {
            return failure(kUsernameTaken, "username already taken");
        }

        const std::string token = newSessionToken();
        store.saveSession(token, username, kSessionLifetime);

        AuthResult result;
        result.ok = true;
        result.sessionToken = token;
        result.username = username;
        return result;
    } catch (const std::exception& error) {
        // Log the cause for the operator, but never the password. The client gets a generic
        // message.
        spdlog::error("register failed: {}", error.what());
        return failure(kServiceUnavailable, "service temporarily unavailable");
    }
}

AuthResult doLogin(store::AccountStore& store, const std::string& rawUsername,
                   const std::string& password) {
    // Every failure below returns the same message. Telling "no such user" apart from
    // "wrong password" would let someone discover which usernames exist.
    const auto invalid = [] {
        return failure(kInvalidCredentials, "invalid username or password");
    };

    if (!core::isValidUsername(rawUsername) || !core::isValidPassword(password)) {
        return invalid();
    }

    const std::string username = toLowercase(rawUsername);
    try {
        const std::optional<std::string> storedHash = store.findPasswordHash(username);

        // Check the password even when the account is missing (against the dummy hash),
        // so the response time does not reveal whether the account exists.
        const bool passwordMatches = verifyPassword(storedHash.value_or(dummyHash()), password);
        if (!storedHash.has_value() || !passwordMatches) {
            return invalid();
        }

        const std::string token = newSessionToken();
        store.saveSession(token, username, kSessionLifetime);

        AuthResult result;
        result.ok = true;
        result.sessionToken = token;
        result.username = username;
        return result;
    } catch (const std::exception& error) {
        spdlog::error("login failed: {}", error.what());
        return failure(kServiceUnavailable, "service temporarily unavailable");
    }
}

}  // namespace

AuthService::AuthService(store::AccountStore& store, boost::asio::thread_pool& workers)
    : m_store(store), m_workers(workers) {}

void AuthService::registerAccount(std::string username, std::string password,
                                  AuthCallback callback) {
    // The password is moved into the task and never stored anywhere else.
    boost::asio::post(m_workers,
                      [this, username = std::move(username), password = std::move(password),
                       callback = std::move(callback)]() mutable {
                          callback(doRegister(m_store, username, password));
                      });
}

void AuthService::login(std::string username, std::string password, AuthCallback callback) {
    boost::asio::post(m_workers,
                      [this, username = std::move(username), password = std::move(password),
                       callback = std::move(callback)]() mutable {
                          callback(doLogin(m_store, username, password));
                      });
}

}  // namespace matchforge::auth

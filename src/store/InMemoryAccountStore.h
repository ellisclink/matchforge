// InMemoryAccountStore.h: an AccountStore that keeps everything in RAM.
// Used by the unit tests and for local experiments. Data is lost when the process exits.
#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "store/AccountStore.h"

namespace matchforge::store {

class InMemoryAccountStore : public AccountStore {
public:
    bool createAccount(std::string_view username, std::string_view passwordHash) override;
    std::optional<std::string> findPasswordHash(std::string_view username) override;
    void saveSession(std::string_view token, std::string_view username,
                     std::chrono::seconds ttl) override;
    std::optional<std::string> findSessionUser(std::string_view token) override;

private:
    struct Session {
        std::string username;
        std::chrono::steady_clock::time_point expiresAt;  // steady_clock never jumps backwards
    };

    // Worker threads may call the store at the same time, so every access takes this lock.
    std::mutex m_mutex;
    std::unordered_map<std::string, std::string> m_passwordHashes;  // username -> hash
    std::unordered_map<std::string, Session> m_sessions;            // token -> session
};

}  // namespace matchforge::store

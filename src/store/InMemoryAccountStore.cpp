// InMemoryAccountStore.cpp: the in-memory implementation of AccountStore.
#include "store/InMemoryAccountStore.h"

#include <string>

namespace matchforge::store {

bool InMemoryAccountStore::createAccount(std::string_view username, std::string_view passwordHash) {
    std::lock_guard lock(m_mutex);
    // emplace does nothing if the key exists, and reports whether it inserted.
    const auto [it, inserted] =
        m_passwordHashes.emplace(std::string(username), std::string(passwordHash));
    return inserted;
}

std::optional<std::string> InMemoryAccountStore::findPasswordHash(std::string_view username) {
    std::lock_guard lock(m_mutex);
    const auto it = m_passwordHashes.find(std::string(username));
    if (it == m_passwordHashes.end()) {
        return std::nullopt;
    }
    return it->second;
}

void InMemoryAccountStore::saveSession(std::string_view token, std::string_view username,
                                       std::chrono::seconds ttl) {
    std::lock_guard lock(m_mutex);
    m_sessions[std::string(token)] = Session{
        std::string(username),
        std::chrono::steady_clock::now() + ttl,
    };
}

std::optional<std::string> InMemoryAccountStore::findSessionUser(std::string_view token) {
    std::lock_guard lock(m_mutex);
    const auto it = m_sessions.find(std::string(token));
    if (it == m_sessions.end()) {
        return std::nullopt;
    }
    if (std::chrono::steady_clock::now() >= it->second.expiresAt) {
        m_sessions.erase(it);  // Expired: forget it, so the map does not grow forever.
        return std::nullopt;
    }
    return it->second.username;
}

std::optional<int> InMemoryAccountStore::findRating(std::string_view username) {
    std::lock_guard lock(m_mutex);
    const auto it = m_ratings.find(std::string(username));
    if (it == m_ratings.end()) {
        return std::nullopt;
    }
    return it->second;
}

void InMemoryAccountStore::saveRating(std::string_view username, int rating) {
    std::lock_guard lock(m_mutex);
    m_ratings[std::string(username)] = rating;
}

int InMemoryAccountStore::adjustRating(std::string_view username, int delta, int startingRating) {
    std::lock_guard lock(m_mutex);  // The lock makes read-modify-write one step.
    // try_emplace inserts startingRating only when the player has no rating yet.
    auto [it, inserted] = m_ratings.try_emplace(std::string(username), startingRating);
    it->second += delta;
    return it->second;
}

}  // namespace matchforge::store

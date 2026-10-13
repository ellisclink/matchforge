// LoginThrottle.cpp: failure counting and lockout.
#include "core/LoginThrottle.h"

namespace matchforge::core {

namespace {

// Clean up stale entries once the map reaches this size. Cleaning on every call would be
// wasteful; never cleaning would let an attacker fill memory with random usernames.
constexpr std::size_t kPruneThreshold = 10000;

}  // namespace

LoginThrottle::LoginThrottle(LoginThrottleSettings settings) : m_settings(settings) {}

bool LoginThrottle::isLocked(const std::string& username, Clock::time_point now) {
    const auto it = m_entries.find(username);
    return it != m_entries.end() && now < it->second.lockedUntil;
}

void LoginThrottle::recordFailure(const std::string& username, Clock::time_point now) {
    if (m_entries.size() >= kPruneThreshold) {
        prune(now);
    }

    Entry& entry = m_entries[username];
    // Start a new window if this is the first failure or the old window has passed.
    if (entry.failures == 0 || now - entry.firstFailure > m_settings.window) {
        entry.failures = 0;
        entry.firstFailure = now;
    }
    ++entry.failures;

    if (entry.failures >= m_settings.maxFailures) {
        entry.lockedUntil = now + m_settings.lockout;
        entry.failures = 0;  // After the lockout, the user gets a fresh set of attempts.
    }
}

void LoginThrottle::recordSuccess(const std::string& username) { m_entries.erase(username); }

void LoginThrottle::prune(Clock::time_point now) {
    for (auto it = m_entries.begin(); it != m_entries.end();) {
        const bool locked = now < it->second.lockedUntil;
        const bool windowOpen =
            it->second.failures > 0 && now - it->second.firstFailure <= m_settings.window;
        if (!locked && !windowOpen) {
            it = m_entries.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace matchforge::core

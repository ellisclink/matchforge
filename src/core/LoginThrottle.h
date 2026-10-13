// LoginThrottle.h: slows down password guessing.
//
// After maxFailures wrong passwords for one username within `window`, that username is
// locked for `lockout`. During the lockout every login attempt is refused without even
// checking the password, so an attacker gets only a handful of guesses per lockout period.
//
// Trade-off: someone who knows a username can lock its owner out for a few minutes by
// failing on purpose. A short lockout keeps that annoyance small; per-IP limits would be
// the next step.
//
// Pure logic with the time passed in. Not thread-safe; the auth service guards it with a mutex.
#pragma once

#include <chrono>
#include <cstddef>
#include <string>
#include <unordered_map>

namespace matchforge::core {

struct LoginThrottleSettings {
    int maxFailures = 5;                // wrong passwords allowed in one window
    std::chrono::seconds window{300};   // failures older than this are forgotten
    std::chrono::seconds lockout{300};  // how long the username stays locked
};

class LoginThrottle {
public:
    using Clock = std::chrono::steady_clock;

    explicit LoginThrottle(LoginThrottleSettings settings = {});

    // True if logins for this username are refused right now.
    bool isLocked(const std::string& username, Clock::time_point now);

    // Counts a wrong password. Locks the username when the limit is reached.
    void recordFailure(const std::string& username, Clock::time_point now);

    // A correct password clears the failure count.
    void recordSuccess(const std::string& username);

    // How many usernames are being tracked (for tests).
    std::size_t trackedCount() const { return m_entries.size(); }

private:
    struct Entry {
        int failures = 0;
        Clock::time_point firstFailure{};
        Clock::time_point lockedUntil{};
    };

    // Drops entries that are neither locked nor inside a failure window, so the map
    // cannot grow without bound when many usernames are tried.
    void prune(Clock::time_point now);

    LoginThrottleSettings m_settings;
    std::unordered_map<std::string, Entry> m_entries;
};

}  // namespace matchforge::core

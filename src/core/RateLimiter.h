// RateLimiter.h: a token bucket, used to stop one client from flooding the server.
//
// Picture a bucket that holds up to `burst` tokens and is refilled at `ratePerSecond`.
// Each message takes one token. A client sending at the normal rate never runs out;
// a short burst is fine because the bucket starts full; a flood empties it and further
// messages are refused until it refills.
//
// Pure logic: the caller passes the time in. Not thread-safe; each session owns one and
// uses it only on its own strand.
#pragma once

#include <chrono>

namespace matchforge::core {

class RateLimiter {
public:
    using Clock = std::chrono::steady_clock;

    // ratePerSecond: long-run messages allowed per second. burst: how many may arrive at once.
    RateLimiter(double ratePerSecond, double burst);

    // Takes one token if there is one. Returns false when the caller is over the limit.
    bool allow(Clock::time_point now);

private:
    double m_ratePerSecond;
    double m_burst;
    double m_tokens;
    Clock::time_point m_lastRefill{};
    bool m_started = false;  // The first call sets the starting time.
};

}  // namespace matchforge::core

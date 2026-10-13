// RateLimiter.cpp: token bucket refill and spend.
#include "core/RateLimiter.h"

#include <algorithm>

namespace matchforge::core {

RateLimiter::RateLimiter(double ratePerSecond, double burst)
    : m_ratePerSecond(ratePerSecond), m_burst(burst), m_tokens(burst) {}

bool RateLimiter::allow(Clock::time_point now) {
    if (!m_started) {
        m_started = true;
        m_lastRefill = now;
    }

    // Add the tokens earned since the last call, but never more than the bucket holds.
    // Refilling lazily here means no timer is needed.
    const std::chrono::duration<double> elapsed = now - m_lastRefill;
    if (elapsed.count() > 0) {
        m_tokens = std::min(m_burst, m_tokens + elapsed.count() * m_ratePerSecond);
        m_lastRefill = now;
    }

    if (m_tokens >= 1.0) {
        m_tokens -= 1.0;
        return true;
    }
    return false;
}

}  // namespace matchforge::core

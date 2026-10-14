// RateLimiterTest.cpp: the token bucket, with time passed in.
#include <gtest/gtest.h>

#include <chrono>

#include "core/RateLimiter.h"

using matchforge::core::RateLimiter;
using namespace std::chrono_literals;

namespace {
const RateLimiter::Clock::time_point kStart{};
}

TEST(RateLimiterTest, AllowsABurstThenRefuses) {
    RateLimiter limiter(/*ratePerSecond=*/1, /*burst=*/3);

    EXPECT_TRUE(limiter.allow(kStart));
    EXPECT_TRUE(limiter.allow(kStart));
    EXPECT_TRUE(limiter.allow(kStart));
    EXPECT_FALSE(limiter.allow(kStart));  // The bucket is empty.
}

TEST(RateLimiterTest, RefillsOverTime) {
    RateLimiter limiter(/*ratePerSecond=*/2, /*burst=*/2);
    ASSERT_TRUE(limiter.allow(kStart));
    ASSERT_TRUE(limiter.allow(kStart));
    ASSERT_FALSE(limiter.allow(kStart));

    // Half a second at 2 tokens per second earns exactly one token.
    EXPECT_TRUE(limiter.allow(kStart + 500ms));
    EXPECT_FALSE(limiter.allow(kStart + 500ms));
}

TEST(RateLimiterTest, NeverHoldsMoreThanTheBurst) {
    RateLimiter limiter(/*ratePerSecond=*/10, /*burst=*/2);
    limiter.allow(kStart);

    // A long quiet time does not bank more than `burst` tokens.
    const auto later = kStart + 1h;
    EXPECT_TRUE(limiter.allow(later));
    EXPECT_TRUE(limiter.allow(later));
    EXPECT_FALSE(limiter.allow(later));
}

TEST(RateLimiterTest, SteadyTrafficAtTheRateIsAlwaysAllowed) {
    RateLimiter limiter(/*ratePerSecond=*/10, /*burst=*/1);
    for (int i = 0; i < 100; ++i) {
        // One message every 100 ms is exactly 10 per second.
        EXPECT_TRUE(limiter.allow(kStart + i * 100ms)) << "message " << i;
    }
}

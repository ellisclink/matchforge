// LoginThrottleTest.cpp: failed-login counting and lockout, with time passed in.
#include <gtest/gtest.h>

#include <chrono>
#include <string>

#include "core/LoginThrottle.h"

using matchforge::core::LoginThrottle;
using matchforge::core::LoginThrottleSettings;
using namespace std::chrono_literals;

namespace {
const LoginThrottle::Clock::time_point kStart{};
const LoginThrottleSettings kSettings{/*maxFailures=*/3, /*window=*/60s, /*lockout=*/120s};
}  // namespace

TEST(LoginThrottleTest, LocksAfterTooManyFailures) {
    LoginThrottle throttle(kSettings);

    throttle.recordFailure("alice", kStart);
    throttle.recordFailure("alice", kStart + 1s);
    EXPECT_FALSE(throttle.isLocked("alice", kStart + 2s));  // 2 of 3: still allowed.

    throttle.recordFailure("alice", kStart + 2s);
    EXPECT_TRUE(throttle.isLocked("alice", kStart + 3s));
}

TEST(LoginThrottleTest, LockExpiresAfterTheLockoutPeriod) {
    LoginThrottle throttle(kSettings);
    for (int i = 0; i < 3; ++i) {
        throttle.recordFailure("alice", kStart);
    }

    EXPECT_TRUE(throttle.isLocked("alice", kStart + 119s));
    EXPECT_FALSE(throttle.isLocked("alice", kStart + 120s));
}

TEST(LoginThrottleTest, FailuresOutsideTheWindowAreForgotten) {
    LoginThrottle throttle(kSettings);

    throttle.recordFailure("alice", kStart);
    throttle.recordFailure("alice", kStart + 1s);
    // The third failure is 2 minutes later, outside the 60-second window: a fresh count.
    throttle.recordFailure("alice", kStart + 120s);

    EXPECT_FALSE(throttle.isLocked("alice", kStart + 121s));
}

TEST(LoginThrottleTest, ASuccessfulLoginClearsTheCount) {
    LoginThrottle throttle(kSettings);
    throttle.recordFailure("alice", kStart);
    throttle.recordFailure("alice", kStart);

    throttle.recordSuccess("alice");
    throttle.recordFailure("alice", kStart + 1s);

    EXPECT_FALSE(throttle.isLocked("alice", kStart + 2s));
    EXPECT_EQ(throttle.trackedCount(), 1u);
}

TEST(LoginThrottleTest, OtherUsernamesAreNotAffected) {
    LoginThrottle throttle(kSettings);
    for (int i = 0; i < 3; ++i) {
        throttle.recordFailure("alice", kStart);
    }

    EXPECT_TRUE(throttle.isLocked("alice", kStart));
    EXPECT_FALSE(throttle.isLocked("bob", kStart));
}

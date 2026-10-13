// RedisAccountStoreTest.cpp: the Redis AccountStore against a real Redis server.
//
// These tests need Redis. They are skipped unless MATCHFORGE_TEST_REDIS_PORT is set,
// so `ctest` still passes on a machine with no Redis. To run them, start Redis and set
// the variables, for example:
//   docker run -d --name mf-test-redis -p 6380:6379 redis:7.4-alpine
//   MATCHFORGE_TEST_REDIS_PORT=6380 ctest --preset debug -R RedisAccountStore
#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <random>
#include <string>

#include "store/RedisAccountStore.h"

namespace {

// Returns the Redis port from the environment, or an empty string when tests should be skipped.
std::string testRedisPort() {
    const char* value = std::getenv("MATCHFORGE_TEST_REDIS_PORT");
    return value != nullptr ? std::string(value) : std::string();
}

// A username that no earlier test run used, so reruns against the same Redis do not clash.
std::string uniqueName(const char* prefix) {
    std::random_device device;
    return std::string(prefix) + "_" + std::to_string(device());
}

}  // namespace

TEST(RedisAccountStoreTest, CreatesAccountsOnceAndReadsTheHashBack) {
    const std::string port = testRedisPort();
    if (port.empty()) {
        GTEST_SKIP() << "set MATCHFORGE_TEST_REDIS_PORT to run the Redis tests";
    }

    matchforge::store::RedisAccountStore store("127.0.0.1", port);
    const std::string name = uniqueName("redistest");

    EXPECT_TRUE(store.createAccount(name, "hash-value"));
    EXPECT_FALSE(store.createAccount(name, "other-hash"));  // SETNX: the name is already taken.
    EXPECT_EQ(store.findPasswordHash(name), "hash-value");
    EXPECT_FALSE(store.findPasswordHash(name + "_missing").has_value());
}

TEST(RedisAccountStoreTest, SessionsRoundTripThroughRedis) {
    const std::string port = testRedisPort();
    if (port.empty()) {
        GTEST_SKIP() << "set MATCHFORGE_TEST_REDIS_PORT to run the Redis tests";
    }

    matchforge::store::RedisAccountStore store("127.0.0.1", port);
    const std::string token = uniqueName("token");

    store.saveSession(token, "alice", std::chrono::minutes(5));

    EXPECT_EQ(store.findSessionUser(token), "alice");
}

TEST(RedisAccountStoreTest, ThrowsWhenRedisIsUnreachable) {
    // Port 1 is never open for Redis, so every command fails. The error must be reported as an
    // exception, which the auth code turns into a 503 for the client.
    matchforge::store::RedisAccountStore store("127.0.0.1", "1");

    EXPECT_THROW(store.findPasswordHash("anyone"), std::runtime_error);
}

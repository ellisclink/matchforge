// InMemoryAccountStoreTest.cpp: the in-memory AccountStore, which the other tests rely on.
#include <gtest/gtest.h>

#include <chrono>

#include "store/InMemoryAccountStore.h"

using matchforge::store::InMemoryAccountStore;

TEST(InMemoryAccountStoreTest, CreateAccountOnlyOncePerUsername) {
    InMemoryAccountStore store;

    EXPECT_TRUE(store.createAccount("alice", "hash-1"));
    EXPECT_FALSE(store.createAccount("alice", "hash-2"));  // Already taken: the first hash stays.
    EXPECT_EQ(store.findPasswordHash("alice"), "hash-1");
}

TEST(InMemoryAccountStoreTest, FindPasswordHashReturnsNothingForUnknownUsers) {
    InMemoryAccountStore store;

    EXPECT_FALSE(store.findPasswordHash("ghost").has_value());
}

TEST(InMemoryAccountStoreTest, SessionMapsTokenToUsername) {
    InMemoryAccountStore store;

    store.saveSession("token-abc", "alice", std::chrono::hours(1));

    EXPECT_EQ(store.findSessionUser("token-abc"), "alice");
    EXPECT_FALSE(store.findSessionUser("token-xyz").has_value());
}

TEST(InMemoryAccountStoreTest, ExpiredSessionsAreForgotten) {
    InMemoryAccountStore store;

    // A zero-second lifetime is already expired by the time we look.
    store.saveSession("short-lived", "alice", std::chrono::seconds(0));

    EXPECT_FALSE(store.findSessionUser("short-lived").has_value());
}

TEST(InMemoryAccountStoreTest, RatingsAreSavedAndRead) {
    InMemoryAccountStore store;

    EXPECT_FALSE(store.findRating("alice").has_value());  // New player: nothing saved.
    store.saveRating("alice", 1620);
    EXPECT_EQ(store.findRating("alice"), 1620);
}

TEST(InMemoryAccountStoreTest, AdjustRatingStartsFromTheStartingRating) {
    InMemoryAccountStore store;

    EXPECT_EQ(store.adjustRating("newbie", +16, 1500), 1516);  // No saved rating: 1500 + 16.
    EXPECT_EQ(store.adjustRating("newbie", -10, 1500), 1506);  // Then applied to the saved value.
}

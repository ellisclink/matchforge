// PasswordHasherTest.cpp: Argon2id hashing and session tokens.
#include <gtest/gtest.h>

#include <set>
#include <string>

#include "auth/PasswordHasher.h"

// The right password verifies, and nothing else does.
TEST(PasswordHasherTest, VerifiesTheCorrectPasswordOnly) {
    const std::string hash = matchforge::auth::hashPassword("correct horse");

    EXPECT_TRUE(matchforge::auth::verifyPassword(hash, "correct horse"));
    EXPECT_FALSE(matchforge::auth::verifyPassword(hash, "wrong horse"));
    EXPECT_FALSE(matchforge::auth::verifyPassword(hash, ""));
}

// The stored hash must not contain the password in plain text.
TEST(PasswordHasherTest, HashDoesNotContainThePlainPassword) {
    const std::string password = "supersecret-password";
    const std::string hash = matchforge::auth::hashPassword(password);

    EXPECT_EQ(hash.find(password), std::string::npos);
    EXPECT_EQ(hash.rfind("$argon2id$", 0), 0u);  // The hash names its algorithm.
}

// A random salt means the same password gives a different hash each time.
TEST(PasswordHasherTest, SamePasswordGivesDifferentHashes) {
    EXPECT_NE(matchforge::auth::hashPassword("same password"),
              matchforge::auth::hashPassword("same password"));
}

// 32 random bytes in URL-safe base64 with no padding is 43 characters.
TEST(PasswordHasherTest, SessionTokensAreLongAndUnique) {
    std::set<std::string> tokens;
    for (int i = 0; i < 50; ++i) {
        const std::string token = matchforge::auth::newSessionToken();
        EXPECT_EQ(token.size(), 43u);
        EXPECT_EQ(token.find('='), std::string::npos);  // No padding characters.
        tokens.insert(token);
    }
    EXPECT_EQ(tokens.size(), 50u);  // No two tokens were equal.
}

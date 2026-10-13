// ValidationTest.cpp: the username and password rules. Pure functions, no setup needed.
#include <gtest/gtest.h>

#include <string>

#include "core/Validation.h"

TEST(ValidationTest, AcceptsUsernamesWithLettersDigitsAndUnderscore) {
    EXPECT_TRUE(matchforge::core::isValidUsername("abc"));  // Shortest allowed: 3.
    EXPECT_TRUE(matchforge::core::isValidUsername("Player_42"));
    EXPECT_TRUE(matchforge::core::isValidUsername(std::string(20, 'x')));  // Longest allowed: 20.
}

TEST(ValidationTest, RejectsUsernamesOutsideTheLengthLimits) {
    EXPECT_FALSE(matchforge::core::isValidUsername(""));
    EXPECT_FALSE(matchforge::core::isValidUsername("ab"));                  // 2: too short.
    EXPECT_FALSE(matchforge::core::isValidUsername(std::string(21, 'x')));  // 21: too long.
}

TEST(ValidationTest, RejectsUsernamesWithSpacesSymbolsOrNonAscii) {
    EXPECT_FALSE(matchforge::core::isValidUsername("has space"));
    EXPECT_FALSE(matchforge::core::isValidUsername("name!"));
    EXPECT_FALSE(matchforge::core::isValidUsername("a/b"));
    EXPECT_FALSE(matchforge::core::isValidUsername("caf\xc3\xa9"));  // UTF-8 bytes are not allowed.
}

TEST(ValidationTest, AcceptsPasswordsOfEightToOneHundredTwentyEightBytes) {
    EXPECT_TRUE(matchforge::core::isValidPassword("12345678"));               // Exactly 8.
    EXPECT_TRUE(matchforge::core::isValidPassword("correct horse battery"));  // Spaces are fine.
    EXPECT_TRUE(matchforge::core::isValidPassword(std::string(128, 'p')));    // Exactly 128.
}

TEST(ValidationTest, RejectsPasswordsOutsideTheLengthLimits) {
    EXPECT_FALSE(matchforge::core::isValidPassword("1234567"));              // 7: too short.
    EXPECT_FALSE(matchforge::core::isValidPassword(std::string(129, 'p')));  // 129: too long.
}

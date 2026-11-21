// BotOptionsTest.cpp: command-line parsing for the bot simulator.
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "bots/BotOptions.h"

using matchforge::bots::parseBotOptions;

TEST(BotOptionsTest, NoArgumentsGiveTheDefaults) {
    std::string error;
    const auto options = parseBotOptions({}, error);

    ASSERT_TRUE(options.has_value());
    EXPECT_EQ(options->host, "127.0.0.1");
    EXPECT_EQ(options->port, 7777);
    EXPECT_EQ(options->bots, 10);
    EXPECT_EQ(options->rounds, 1);
    EXPECT_EQ(options->mode, "duel");
    EXPECT_TRUE(options->resultSecret.empty());
}

TEST(BotOptionsTest, ReadsEveryOption) {
    std::string error;
    const auto options =
        parseBotOptions({"--host", "server", "--port", "9000", "--bots", "50", "--rounds", "3",
                         "--mode", "duo", "--region", "na", "--secret", "s3cret", "--timeout", "5"},
                        error);

    ASSERT_TRUE(options.has_value()) << error;
    EXPECT_EQ(options->host, "server");
    EXPECT_EQ(options->port, 9000);
    EXPECT_EQ(options->bots, 50);
    EXPECT_EQ(options->rounds, 3);
    EXPECT_EQ(options->mode, "duo");
    EXPECT_EQ(options->region, "na");
    EXPECT_EQ(options->resultSecret, "s3cret");
    EXPECT_EQ(options->timeoutSeconds, 5);
}

TEST(BotOptionsTest, RejectsBadNumbers) {
    std::string error;
    EXPECT_FALSE(parseBotOptions({"--bots", "0"}, error).has_value());
    EXPECT_FALSE(parseBotOptions({"--bots", "lots"}, error).has_value());
    EXPECT_FALSE(parseBotOptions({"--port", "70000"}, error).has_value());
    EXPECT_FALSE(parseBotOptions({"--rounds", "5x"}, error).has_value());
    EXPECT_NE(error.find("--rounds"), std::string::npos);  // The message names the bad option.
}

TEST(BotOptionsTest, RejectsUnknownOptionsAndMissingValues) {
    std::string error;
    EXPECT_FALSE(parseBotOptions({"--speed", "fast"}, error).has_value());
    EXPECT_NE(error.find("unknown option"), std::string::npos);

    EXPECT_FALSE(parseBotOptions({"--bots"}, error).has_value());
    EXPECT_NE(error.find("missing value"), std::string::npos);
}

TEST(BotOptionsTest, HelpReturnsTheUsageText) {
    std::string error;
    EXPECT_FALSE(parseBotOptions({"--help"}, error).has_value());
    EXPECT_EQ(error, matchforge::bots::botUsage());
}

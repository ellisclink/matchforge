// ServerConfigTest.cpp: loading and validating the JSON settings file.
#include <gtest/gtest.h>

#include <string>

#include "config/ServerConfig.h"

using matchforge::config::ConfigError;
using matchforge::config::loadConfigFile;
using matchforge::config::parseConfig;

namespace {

// Returns the ConfigError message for `json`, or "" if it parsed without error.
std::string errorFor(const std::string& json) {
    try {
        parseConfig(json);
    } catch (const ConfigError& error) {
        return error.what();
    }
    return "";
}

}  // namespace

TEST(ServerConfigTest, AnEmptyObjectGivesTheDefaults) {
    const auto config = parseConfig("{}");

    EXPECT_EQ(config.port, 7777);
    EXPECT_EQ(config.maxFrameBytes, 65536u);
    EXPECT_EQ(config.kFactor, 32);
    EXPECT_EQ(config.regions.size(), 3u);
    EXPECT_EQ(config.modes.size(), 3u);
    EXPECT_EQ(config.idleTimeout.count(), 60);
}

TEST(ServerConfigTest, TheShippedConfigFileIsValid) {
    // MATCHFORGE_SOURCE_DIR is set by tests/CMakeLists.txt to the repository root.
    const auto config = loadConfigFile(std::string(MATCHFORGE_SOURCE_DIR) + "/config/server.json");

    EXPECT_EQ(config.port, 7777);
    EXPECT_EQ(config.metricsPort, 9100);
    EXPECT_EQ(config.modes[1].name, "duo");
    EXPECT_EQ(config.modes[1].teamSize, 2);
    EXPECT_EQ(config.tickInterval.count(), 250);
}

TEST(ServerConfigTest, ReadsNestedValues) {
    const auto config = parseConfig(R"({
        "port": 9000,
        "redis": {"host": "redis", "port": 6380},
        "matchmaking": {"kFactor": 24, "regions": ["eu"], "modes": [{"name": "duel", "teamSize": 1}]},
        "limits": {"messagesPerSecond": 5, "messageBurst": 10, "loginLockoutSeconds": 30}
    })");

    EXPECT_EQ(config.port, 9000);
    EXPECT_EQ(config.redisHost, "redis");
    EXPECT_EQ(config.redisPort, "6380");
    EXPECT_EQ(config.kFactor, 24);
    ASSERT_EQ(config.regions.size(), 1u);
    EXPECT_EQ(config.regions[0], "eu");
    EXPECT_DOUBLE_EQ(config.messagesPerSecond, 5);
    EXPECT_EQ(config.loginLockout.count(), 30);
}

TEST(ServerConfigTest, UnknownKeysAreErrorsSoTyposAreCaught) {
    EXPECT_NE(errorFor(R"({"prot": 7777})").find("unknown setting 'prot'"), std::string::npos);
    EXPECT_NE(errorFor(R"({"matchmaking": {"kfactor": 32}})").find("'matchmaking.kfactor'"),
              std::string::npos);
}

TEST(ServerConfigTest, WrongTypesAreErrors) {
    EXPECT_NE(errorFor(R"({"port": "7777"})").find("whole number"), std::string::npos);
    EXPECT_NE(errorFor(R"({"matchmaking": {"regions": "eu"}})").find("list"), std::string::npos);
}

TEST(ServerConfigTest, OutOfRangeValuesAreErrors) {
    EXPECT_NE(errorFor(R"({"port": 70000})").find("between 1 and 65535"), std::string::npos);
    EXPECT_NE(errorFor(R"({"matchmaking": {"kFactor": 0}})").find("kFactor"), std::string::npos);
    EXPECT_NE(errorFor(R"({"maxFrameBytes": 10})").find("maxFrameBytes"), std::string::npos);
}

TEST(ServerConfigTest, ValuesThatContradictEachOtherAreErrors) {
    EXPECT_NE(errorFor(R"({"matchmaking": {"baseRatingWindow": 700, "maxRatingWindow": 600}})")
                  .find("baseRatingWindow"),
              std::string::npos);
    EXPECT_NE(errorFor(R"({"limits": {"messagesPerSecond": 50, "messageBurst": 10}})")
                  .find("messageBurst"),
              std::string::npos);
    EXPECT_NE(errorFor(R"({"port": 9100, "metricsPort": 9100})").find("metricsPort"),
              std::string::npos);
}

TEST(ServerConfigTest, DuplicateOrBadNamesAreErrors) {
    EXPECT_NE(errorFor(R"({"matchmaking": {"regions": ["eu", "eu"]}})").find("listed twice"),
              std::string::npos);
    EXPECT_NE(errorFor(R"({"matchmaking": {"modes": [{"name": "duel"}, {"name": "duel"}]}})")
                  .find("listed twice"),
              std::string::npos);
    EXPECT_NE(errorFor(R"({"matchmaking": {"regions": ["eu west"]}})").find("region 'eu west'"),
              std::string::npos);
    EXPECT_NE(errorFor(R"({"matchmaking": {"regions": []}})").find("at least one region"),
              std::string::npos);
}

TEST(ServerConfigTest, InvalidJsonAndMissingFilesAreErrors) {
    EXPECT_NE(errorFor("{ not json").find("not valid JSON"), std::string::npos);
    EXPECT_NE(errorFor("[1, 2]").find("JSON object"), std::string::npos);
    EXPECT_THROW(loadConfigFile("/does/not/exist.json"), ConfigError);
}

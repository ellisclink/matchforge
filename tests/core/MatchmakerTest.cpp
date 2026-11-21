// MatchmakerTest.cpp: the queue and the matching rules. Time is passed in, so every
// test controls exactly how long each player has waited.
#include <gtest/gtest.h>

#include <chrono>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/Matchmaker.h"

using matchforge::core::Clock;
using matchforge::core::JoinResult;
using matchforge::core::Match;
using matchforge::core::Matchmaker;
using matchforge::core::MatchMode;
using matchforge::core::MatchSettings;
using matchforge::core::QueuedPlayer;

namespace {

// A fixed starting instant. Tests add seconds to it instead of reading a real clock.
const Clock::time_point kStart{};

Matchmaker makeMatchmaker() {
    // Settings: +-100 at first, growing 10 per second, never past 600.
    return Matchmaker({{"duel", 1}, {"duo", 2}}, {"eu", "na"}, MatchSettings{100, 10, 600});
}

QueuedPlayer player(const std::string& name, int rating, const std::string& mode = "duel",
                    const std::string& region = "eu", Clock::time_point joinedAt = kStart) {
    return QueuedPlayer{name, rating, mode, region, joinedAt};
}

int teamTotal(const std::vector<QueuedPlayer>& team) {
    return std::accumulate(team.begin(), team.end(), 0,
                           [](int sum, const QueuedPlayer& p) { return sum + p.rating; });
}

}  // namespace

TEST(MatchmakerTest, TwoClosePlayersAreMatched) {
    Matchmaker mm = makeMatchmaker();
    ASSERT_EQ(mm.join(player("alice", 1500)), JoinResult::Joined);
    ASSERT_EQ(mm.join(player("bob", 1550)), JoinResult::Joined);

    const std::vector<Match> matches = mm.findMatches(kStart);

    ASSERT_EQ(matches.size(), 1u);
    EXPECT_EQ(matches[0].mode, "duel");
    EXPECT_EQ(matches[0].region, "eu");
    ASSERT_EQ(matches[0].teamA.size(), 1u);
    ASSERT_EQ(matches[0].teamB.size(), 1u);
    EXPECT_EQ(mm.queuedCount(), 0u);  // Matched players leave the queue.
}

TEST(MatchmakerTest, OnePlayerAloneIsNotMatched) {
    Matchmaker mm = makeMatchmaker();
    mm.join(player("alice", 1500));

    EXPECT_TRUE(mm.findMatches(kStart).empty());
    EXPECT_TRUE(mm.isQueued("alice"));
}

TEST(MatchmakerTest, DifferentRegionsAreNeverMatched) {
    Matchmaker mm = makeMatchmaker();
    mm.join(player("alice", 1500, "duel", "eu"));
    mm.join(player("bob", 1500, "duel", "na"));

    EXPECT_TRUE(mm.findMatches(kStart + std::chrono::hours(1)).empty());
}

TEST(MatchmakerTest, DifferentModesAreNeverMatched) {
    Matchmaker mm = makeMatchmaker();
    mm.join(player("alice", 1500, "duel"));
    mm.join(player("bob", 1500, "duo"));

    EXPECT_TRUE(mm.findMatches(kStart + std::chrono::hours(1)).empty());
}

TEST(MatchmakerTest, LargeGapWaitsUntilTheWindowHasGrown) {
    Matchmaker mm = makeMatchmaker();
    mm.join(player("alice", 1500));
    mm.join(player("bob", 1800));  // 300 apart: needs a window of 300.

    // At join time the window is 100, so no match.
    EXPECT_TRUE(mm.findMatches(kStart).empty());
    // After 19 s the window is 100 + 190 = 290: still too narrow.
    EXPECT_TRUE(mm.findMatches(kStart + std::chrono::seconds(19)).empty());
    // After 20 s the window is 100 + 200 = 300: matched.
    EXPECT_EQ(mm.findMatches(kStart + std::chrono::seconds(20)).size(), 1u);
}

TEST(MatchmakerTest, WindowNeverGrowsPastTheMaximum) {
    Matchmaker mm = makeMatchmaker();
    mm.join(player("alice", 1000));
    mm.join(player("bob", 1700));  // 700 apart, above the 600 maximum.

    EXPECT_TRUE(mm.findMatches(kStart + std::chrono::hours(24)).empty());
    EXPECT_EQ(mm.ratingWindow(kStart, kStart + std::chrono::hours(24)), 600);
}

TEST(MatchmakerTest, ALongWaitingPlayerWidensTheWindowForTheGroup) {
    Matchmaker mm = makeMatchmaker();
    // Alice has waited 30 s (window 400). Bob just arrived (window 100).
    mm.join(player("alice", 1500, "duel", "eu", kStart));
    mm.join(player("bob", 1850, "duel", "eu", kStart + std::chrono::seconds(30)));

    // The group uses the widest window, Alice's, so the 350 gap is accepted.
    EXPECT_EQ(mm.findMatches(kStart + std::chrono::seconds(30)).size(), 1u);
}

TEST(MatchmakerTest, ClosestRatingsArePairedFirst) {
    Matchmaker mm = makeMatchmaker();
    mm.join(player("low", 1000));
    mm.join(player("lowish", 1010));
    mm.join(player("high", 1500));

    const std::vector<Match> matches = mm.findMatches(kStart);

    ASSERT_EQ(matches.size(), 1u);
    EXPECT_TRUE(mm.isQueued("high"));  // The outlier keeps waiting.
    EXPECT_FALSE(mm.isQueued("low"));
    EXPECT_FALSE(mm.isQueued("lowish"));
}

TEST(MatchmakerTest, TeamsAreBalancedBySnakeDraft) {
    Matchmaker mm = makeMatchmaker();
    mm.join(player("a", 1300, "duo"));
    mm.join(player("b", 1250, "duo"));
    mm.join(player("c", 1240, "duo"));
    mm.join(player("d", 1210, "duo"));

    const std::vector<Match> matches = mm.findMatches(kStart);

    ASSERT_EQ(matches.size(), 1u);
    ASSERT_EQ(matches[0].teamA.size(), 2u);
    ASSERT_EQ(matches[0].teamB.size(), 2u);
    // Snake draft: 1300 and 1210 vs 1250 and 1240, so 2510 vs 2490.
    EXPECT_EQ(teamTotal(matches[0].teamA), 2510);
    EXPECT_EQ(teamTotal(matches[0].teamB), 2490);
}

TEST(MatchmakerTest, ManyPlayersFormSeveralMatchesAndLeftoversWait) {
    Matchmaker mm = makeMatchmaker();
    for (int i = 0; i < 9; ++i) {
        mm.join(player("p" + std::to_string(i), 1500 + i * 5));
    }

    const std::vector<Match> matches = mm.findMatches(kStart);

    EXPECT_EQ(matches.size(), 4u);    // 9 players -> 4 duels
    EXPECT_EQ(mm.queuedCount(), 1u);  // and one player still waiting
}

TEST(MatchmakerTest, JoiningTwiceKeepsTheOriginalPlace) {
    Matchmaker mm = makeMatchmaker();
    ASSERT_EQ(mm.join(player("alice", 1500, "duel", "eu", kStart)), JoinResult::Joined);

    EXPECT_EQ(mm.join(player("alice", 1500, "duel", "eu", kStart + std::chrono::seconds(50))),
              JoinResult::AlreadyQueued);
    EXPECT_EQ(mm.queuedCount(), 1u);
}

TEST(MatchmakerTest, RejectsUnknownModesAndRegions) {
    Matchmaker mm = makeMatchmaker();

    EXPECT_EQ(mm.join(player("alice", 1500, "battle-royale")), JoinResult::UnknownMode);
    EXPECT_EQ(mm.join(player("alice", 1500, "duel", "mars")), JoinResult::UnknownRegion);
    EXPECT_EQ(mm.queuedCount(), 0u);
}

TEST(MatchmakerTest, LeaveRemovesAPlayer) {
    Matchmaker mm = makeMatchmaker();
    mm.join(player("alice", 1500));

    EXPECT_TRUE(mm.leave("alice"));
    EXPECT_FALSE(mm.leave("alice"));  // Already gone.
    EXPECT_FALSE(mm.isQueued("alice"));
}

TEST(MatchmakerTest, InvalidConfigurationIsRejected) {
    EXPECT_THROW(Matchmaker({{"duel", 0}}, {"eu"}), std::invalid_argument);
    EXPECT_THROW(Matchmaker({{"duel", 1}, {"duel", 2}}, {"eu"}), std::invalid_argument);
    EXPECT_THROW(Matchmaker({{"duel", 1}}, {}), std::invalid_argument);
}

// EloTest.cpp: the Elo formulas against hand-checked values.
#include <gtest/gtest.h>

#include "core/Elo.h"

using matchforge::core::expectedScore;
using matchforge::core::kDraw;
using matchforge::core::kLoss;
using matchforge::core::kWin;
using matchforge::core::ratingDelta;

TEST(EloTest, EqualRatingsAreAFiftyFiftyGame) { EXPECT_DOUBLE_EQ(expectedScore(1500, 1500), 0.5); }

TEST(EloTest, FourHundredPointsAheadIsAboutNinetyOnePercent) {
    // 1 / (1 + 10^-1) = 0.90909...
    EXPECT_NEAR(expectedScore(1900, 1500), 0.90909, 0.0001);
    EXPECT_NEAR(expectedScore(1500, 1900), 0.09091, 0.0001);
}

TEST(EloTest, BothSidesExpectationsAddUpToOne) {
    EXPECT_NEAR(expectedScore(1234, 1700) + expectedScore(1700, 1234), 1.0, 1e-12);
}

TEST(EloTest, WinningAnEvenGameGainsHalfTheKFactor) {
    // K * (1 - 0.5) = 16 with K = 32.
    EXPECT_EQ(ratingDelta(1500, 1500, kWin), 16);
    EXPECT_EQ(ratingDelta(1500, 1500, kLoss), -16);
    EXPECT_EQ(ratingDelta(1500, 1500, kDraw), 0);
}

TEST(EloTest, UpsetsMoveRatingsMoreThanExpectedResults) {
    const int underdogWins = ratingDelta(1400, 1600, kWin);   // expected about 0.24
    const int favouriteWins = ratingDelta(1600, 1400, kWin);  // expected about 0.76
    EXPECT_EQ(underdogWins, 24);
    EXPECT_EQ(favouriteWins, 8);
    EXPECT_GT(underdogWins, favouriteWins);
}

TEST(EloTest, OneOnOneChangesAreZeroSum) {
    // What the winner gains, the loser loses, so total rating in the system stays the same.
    EXPECT_EQ(ratingDelta(1520, 1480, kWin) + ratingDelta(1480, 1520, kLoss), 0);
}

TEST(EloTest, KFactorScalesTheChange) {
    EXPECT_EQ(ratingDelta(1500, 1500, kWin, 16), 8);
    EXPECT_EQ(ratingDelta(1500, 1500, kWin, 64), 32);
}

// BotReportTest.cpp: the average and percentile math used in the bot summary.
#include <gtest/gtest.h>

#include <vector>

#include "bots/BotReport.h"

using matchforge::bots::summarize;

TEST(BotReportTest, EmptyInputGivesZeros) {
    const auto summary = summarize({});
    EXPECT_EQ(summary.count, 0u);
    EXPECT_EQ(summary.average, 0.0);
    EXPECT_EQ(summary.max, 0.0);
}

TEST(BotReportTest, NearestRankPercentilesOnOneToHundred) {
    std::vector<double> samples;
    for (int i = 100; i >= 1; --i) {  // Unsorted on purpose: summarize must sort.
        samples.push_back(i);
    }

    const auto summary = summarize(samples);

    EXPECT_EQ(summary.count, 100u);
    EXPECT_DOUBLE_EQ(summary.average, 50.5);
    EXPECT_DOUBLE_EQ(summary.p50, 50.0);  // rank ceil(0.50 * 100) = 50
    EXPECT_DOUBLE_EQ(summary.p95, 95.0);  // rank ceil(0.95 * 100) = 95
    EXPECT_DOUBLE_EQ(summary.max, 100.0);
}

TEST(BotReportTest, SingleSampleIsEveryPercentile) {
    const auto summary = summarize({42.0});
    EXPECT_DOUBLE_EQ(summary.p50, 42.0);
    EXPECT_DOUBLE_EQ(summary.p95, 42.0);
    EXPECT_DOUBLE_EQ(summary.max, 42.0);
}

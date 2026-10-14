// BotReport.h: turns raw timings from a bot run into summary numbers.
#pragma once

#include <cstddef>
#include <vector>

namespace matchforge::bots {

// Summary of a set of time measurements, in milliseconds.
struct LatencySummary {
    std::size_t count = 0;
    double average = 0;
    double p50 = 0;  // median: half the samples were faster
    double p95 = 0;  // 95% of samples were this fast or faster; shows the slow tail
    double max = 0;
};

// Returns the summary of `samples`. An empty input gives all zeros.
// Percentiles use the nearest-rank method: the value at position ceil(p/100 * n).
LatencySummary summarize(std::vector<double> samples);

}  // namespace matchforge::bots

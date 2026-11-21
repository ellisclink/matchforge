// BotReport.cpp: average and percentiles for the bot run summary.
#include "bots/BotReport.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace matchforge::bots {

namespace {

// Nearest-rank percentile of already-sorted values.
double percentile(const std::vector<double>& sorted, double p) {
    const auto rank =
        static_cast<std::size_t>(std::ceil(p / 100.0 * static_cast<double>(sorted.size())));
    const std::size_t index = rank == 0 ? 0 : rank - 1;  // ranks start at 1; vector indexes at 0
    return sorted[std::min(index, sorted.size() - 1)];
}

}  // namespace

LatencySummary summarize(std::vector<double> samples) {
    LatencySummary summary;
    if (samples.empty()) {
        return summary;
    }
    std::sort(samples.begin(), samples.end());
    summary.count = samples.size();
    summary.average =
        std::accumulate(samples.begin(), samples.end(), 0.0) / static_cast<double>(samples.size());
    summary.p50 = percentile(samples, 50);
    summary.p95 = percentile(samples, 95);
    summary.max = samples.back();
    return summary;
}

}  // namespace matchforge::bots

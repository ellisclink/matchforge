// Matchmaker.cpp: queue management and the matching algorithm.
#include "core/Matchmaker.h"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace matchforge::core {

namespace {

// Sort order inside a bucket: by rating, then by who joined first, then by name. The extra
// keys make the order fully defined, so the same queue always produces the same matches.
bool byRatingThenAge(const QueuedPlayer& a, const QueuedPlayer& b) {
    return std::tie(a.rating, a.joinedAt, a.playerId) < std::tie(b.rating, b.joinedAt, b.playerId);
}

// Splits a group into two teams with total ratings as equal as possible, using a
// "snake draft": strongest to A, next two to B, next two to A, and so on (A B B A A B ...).
// It is not perfect for every input, but it is simple, fast, and close for typical groups.
Match makeMatch(std::vector<QueuedPlayer> group, const std::string& mode,
                const std::string& region) {
    std::sort(group.begin(), group.end(),
              [](const QueuedPlayer& a, const QueuedPlayer& b) { return a.rating > b.rating; });

    Match match;
    match.mode = mode;
    match.region = region;
    for (std::size_t i = 0; i < group.size(); ++i) {
        const std::size_t round = i / 2;
        const bool firstInRound = (i % 2) == 0;
        // Even rounds: A picks first. Odd rounds: B picks first.
        const bool toTeamA = (round % 2 == 0) ? firstInRound : !firstInRound;
        (toTeamA ? match.teamA : match.teamB).push_back(std::move(group[i]));
    }
    return match;
}

}  // namespace

Matchmaker::Matchmaker(std::vector<MatchMode> modes, std::vector<std::string> regions,
                       MatchSettings settings)
    : m_modes(std::move(modes)), m_regions(std::move(regions)), m_settings(settings) {
    if (m_regions.empty()) {
        throw std::invalid_argument("matchmaker needs at least one region");
    }
    std::set<std::string> names;
    for (const MatchMode& mode : m_modes) {
        if (mode.teamSize < 1) {
            throw std::invalid_argument("mode '" + mode.name +
                                        "' must have at least 1 player per team");
        }
        if (!names.insert(mode.name).second) {
            throw std::invalid_argument("mode '" + mode.name + "' is listed twice");
        }
    }
}

JoinResult Matchmaker::join(QueuedPlayer player) {
    if (findMode(player.mode) == nullptr) {
        return JoinResult::UnknownMode;
    }
    if (std::find(m_regions.begin(), m_regions.end(), player.region) == m_regions.end()) {
        return JoinResult::UnknownRegion;
    }
    // try_emplace leaves an existing entry untouched, so joining twice keeps the original
    // join time (the player does not lose their place).
    const auto [it, inserted] = m_queue.try_emplace(player.playerId, std::move(player));
    return inserted ? JoinResult::Joined : JoinResult::AlreadyQueued;
}

bool Matchmaker::leave(const std::string& playerId) { return m_queue.erase(playerId) > 0; }

bool Matchmaker::isQueued(const std::string& playerId) const { return m_queue.count(playerId) > 0; }

std::size_t Matchmaker::queuedCount() const { return m_queue.size(); }

int Matchmaker::ratingWindow(Clock::time_point joinedAt, Clock::time_point now) const {
    const auto waited = std::chrono::duration_cast<std::chrono::seconds>(now - joinedAt).count();
    const long long waitedSeconds =
        std::max<long long>(0, waited);  // Clock skew must not shrink the window.

    // long long avoids overflow for players who have waited a very long time.
    const long long window =
        static_cast<long long>(m_settings.baseRatingWindow) +
        static_cast<long long>(m_settings.windowGrowthPerSecond) * waitedSeconds;
    return static_cast<int>(std::min<long long>(window, m_settings.maxRatingWindow));
}

std::vector<Match> Matchmaker::findMatches(Clock::time_point now) {
    // Players can only meet others in the same mode and region, so split the queue into
    // one bucket per (mode, region) pair and match inside each bucket.
    std::map<std::pair<std::string, std::string>, std::vector<QueuedPlayer>> buckets;
    for (const auto& [id, player] : m_queue) {
        buckets[{player.mode, player.region}].push_back(player);
    }

    std::vector<Match> matches;
    for (auto& [key, players] : buckets) {
        const MatchMode* mode = findMode(key.first);
        const std::size_t groupSize = 2 * static_cast<std::size_t>(mode->teamSize);

        // Sorting by rating puts similar players next to each other, so the best candidate
        // groups are always runs of consecutive players.
        std::sort(players.begin(), players.end(), byRatingThenAge);

        std::size_t i = 0;
        while (i + groupSize <= players.size()) {
            const auto first = players.begin() + static_cast<std::ptrdiff_t>(i);
            const auto last = first + static_cast<std::ptrdiff_t>(groupSize);  // one past the end

            const int spread = (last - 1)->rating - first->rating;

            // The group is accepted when its spread fits the widest window in the group, that
            // is, the window of whoever has waited longest. This guarantees a long-waiting
            // player is eventually matched, at the cost of a slightly wider gap for newcomers.
            int allowed = 0;
            for (auto it = first; it != last; ++it) {
                allowed = std::max(allowed, ratingWindow(it->joinedAt, now));
            }

            if (spread <= allowed) {
                std::vector<QueuedPlayer> group(first, last);
                for (const QueuedPlayer& player : group) {
                    m_queue.erase(player.playerId);  // Matched players leave the queue.
                }
                matches.push_back(makeMatch(std::move(group), key.first, key.second));
                i += groupSize;
            } else {
                ++i;  // Too far apart: try the next run, which drops the lowest-rated player.
            }
        }
    }
    return matches;
}

const MatchMode* Matchmaker::findMode(const std::string& name) const {
    for (const MatchMode& mode : m_modes) {
        if (mode.name == name) {
            return &mode;
        }
    }
    return nullptr;
}

}  // namespace matchforge::core

// Matchmaker.h: the matchmaking queue. Players wait in a queue per mode and region,
// and the matchmaker groups players with similar ratings into matches.
//
// This class is pure logic: no threads, no sockets, no clock. The caller passes in the
// current time, which keeps every decision repeatable in tests. It is not thread-safe;
// the lobby layer runs it on a single strand (CLAUDE.md: shared matchmaking state is
// owned by one strand).
#pragma once

#include <chrono>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "core/Elo.h"

namespace matchforge::core {

using Clock = std::chrono::steady_clock;

// A way to play, for example "duel" (1 vs 1) or "duo" (2 vs 2). Every match has two teams.
struct MatchMode {
    std::string name;
    int teamSize = 1;  // players per team
};

// How strict matching is about rating differences.
//
// A new player is only matched with players close in rating. The longer they wait, the
// wider the accepted gap gets, so nobody waits forever, up to a hard limit that keeps
// matches fair.
struct MatchSettings {
    int baseRatingWindow = 100;      // largest rating spread allowed right after joining
    int windowGrowthPerSecond = 10;  // how much the allowed spread grows per second of waiting
    int maxRatingWindow = 600;       // the spread never grows past this
};

// One player waiting in the queue.
struct QueuedPlayer {
    std::string playerId;  // the player's username
    int rating = kDefaultRating;
    std::string mode;
    std::string region;
    Clock::time_point joinedAt;
};

// A match the matchmaker has formed. Both teams have mode.teamSize players.
struct Match {
    std::string mode;
    std::string region;
    std::vector<QueuedPlayer> teamA;
    std::vector<QueuedPlayer> teamB;
};

enum class JoinResult {
    Joined,         // the player is now waiting
    AlreadyQueued,  // the player was already waiting; nothing changed
    UnknownMode,    // the mode is not one the server offers
    UnknownRegion,  // the region is not one the server offers
};

class Matchmaker {
public:
    // Throws std::invalid_argument if a mode has teamSize below 1, if two modes share a
    // name, or if no regions are given.
    Matchmaker(std::vector<MatchMode> modes, std::vector<std::string> regions,
               MatchSettings settings = {});

    // Adds a player to the queue for their mode and region.
    JoinResult join(QueuedPlayer player);

    // Removes a player from the queue. Returns false if they were not waiting.
    bool leave(const std::string& playerId);

    bool isQueued(const std::string& playerId) const;
    std::size_t queuedCount() const;

    // Forms as many matches as it can at time `now`. Matched players leave the queue.
    // Call it regularly (the lobby calls it a few times per second).
    std::vector<Match> findMatches(Clock::time_point now);

    // The largest rating spread a player who joined at `joinedAt` accepts at time `now`.
    int ratingWindow(Clock::time_point joinedAt, Clock::time_point now) const;

    const std::vector<MatchMode>& modes() const { return m_modes; }
    const std::vector<std::string>& regions() const { return m_regions; }

private:
    const MatchMode* findMode(const std::string& name) const;

    std::vector<MatchMode> m_modes;
    std::vector<std::string> m_regions;
    MatchSettings m_settings;

    // Waiting players by username. std::map keeps a stable order, so results are repeatable.
    std::map<std::string, QueuedPlayer> m_queue;
};

}  // namespace matchforge::core

// LobbyService.h: the matchmaking queue as a network service.
//
// Concurrency design (CLAUDE.md: shared matchmaking state is owned by one strand):
// every piece of lobby state (the Matchmaker, the list of waiting connections, the
// list of reportable matches) is touched only by handlers running on m_strand. Public
// methods do no work themselves; they post a task to the strand. Sessions on any
// thread can call them safely, and no mutex is needed.
//
// Slow work (reading and writing ratings in Redis) runs on the worker pool, never on
// the strand, so the queue keeps moving while storage is busy.
#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>
#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/Elo.h"
#include "core/Matchmaker.h"
#include "lobby/LobbyClient.h"
#include "store/AccountStore.h"

namespace matchforge::lobby {

struct LobbySettings {
    std::vector<core::MatchMode> modes;
    std::vector<std::string> regions;
    core::MatchSettings matchSettings;

    // How often the matchmaker looks for matches. Shorter means faster matches but more CPU.
    std::chrono::milliseconds tickInterval{250};

    // Shared secret a game server must send with a match result. Empty disables reporting,
    // so ratings cannot change at all until an operator sets one.
    std::string resultSecret;

    int kFactor = core::kDefaultKFactor;

    // How long after a match starts its result can still be reported. Older records are
    // dropped, so the list cannot grow without limit.
    std::chrono::minutes resultWindow{120};
};

// Snapshot of lobby numbers, for metrics and tests.
struct LobbyStats {
    std::size_t playersQueued = 0;
    std::size_t matchesAwaitingResult = 0;
    std::size_t matchesCreated = 0;  // total since start
    std::size_t resultsApplied = 0;  // total since start
};

class LobbyService {
public:
    // The store and worker pool must outlive the service. The service must outlive the
    // io_context's run, because pending timer handlers refer to it.
    LobbyService(boost::asio::io_context& ioContext, store::AccountStore& store,
                 boost::asio::thread_pool& workers, LobbySettings settings);

    // Starts the regular matchmaking pass. Throws nothing.
    void start();

    // Stops the matchmaking pass. Players stay queued; nothing more is matched.
    void stop();

    // Puts a logged-in player in the queue. The client receives QueueStatus on success, or
    // ErrorResponse (400 unknown mode or region, 409 already queued, 503 storage down).
    // The player's current rating is read from storage, so it is always up to date.
    void join(std::string username, std::string mode, std::string region,
              std::weak_ptr<LobbyClient> client);

    // Removes the player from the queue. The client receives QueueStatus{queued=false}.
    void leave(std::string username, std::weak_ptr<LobbyClient> client);

    // Called when a connection closes. Removes the player from the queue only if this
    // connection is the one that queued them (another connection may have queued since).
    void disconnect(std::string username, std::weak_ptr<LobbyClient> client);

    // Applies a finished match's result to every player's rating. Only accepted with the
    // right secret. The reporter receives MatchResultAck, or ErrorResponse (403 bad secret
    // or reporting disabled, 404 unknown or already-reported match, 400 bad team, 503).
    void reportResult(std::string matchId, int winningTeam, std::string secret,
                      std::weak_ptr<LobbyClient> reporter);

    // Calls back (on the lobby strand) with current numbers.
    void stats(std::function<void(LobbyStats)> callback);

private:
    struct MatchRecord {
        core::Match match;
        core::Clock::time_point createdAt;
    };

    void scheduleTick();
    void tick();  // one matchmaking pass, on the strand
    void announce(const std::string& matchId, const core::Match& match);
    void doJoin(core::QueuedPlayer player, std::weak_ptr<LobbyClient> client);

    using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;

    Strand m_strand;
    boost::asio::steady_timer m_timer;
    store::AccountStore& m_store;
    boost::asio::thread_pool& m_workers;
    LobbySettings m_settings;
    core::Matchmaker m_matchmaker;
    bool m_running = false;

    // The connection each queued player is waiting on. weak_ptr: the lobby must not keep a
    // closed connection alive. An expired pointer means the player disconnected.
    std::unordered_map<std::string, std::weak_ptr<LobbyClient>> m_waiting;

    // Matches whose result has not been reported yet, by match id.
    std::unordered_map<std::string, MatchRecord> m_matches;

    std::size_t m_matchesCreated = 0;
    std::size_t m_resultsApplied = 0;
};

}  // namespace matchforge::lobby

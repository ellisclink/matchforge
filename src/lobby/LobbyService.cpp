// LobbyService.cpp: queue requests, the regular matchmaking pass, and match results.
#include "lobby/LobbyService.h"

#include <spdlog/spdlog.h>

#include <boost/asio/post.hpp>
#include <boost/system/error_code.hpp>
#include <cstdint>
#include <exception>
#include <numeric>
#include <string_view>
#include <utility>

#include "auth/PasswordHasher.h"
#include "matchforge.pb.h"
#include "metrics/Metrics.h"

namespace matchforge::lobby {

namespace {

// Error codes sent in ErrorResponse. The same numbers are used across the server.
constexpr std::int32_t kBadRequest = 400;
constexpr std::int32_t kForbidden = 403;
constexpr std::int32_t kNotFound = 404;
constexpr std::int32_t kConflict = 409;
constexpr std::int32_t kUnavailable = 503;

// Match ids are 16 characters of a random token: 96 random bits, so ids cannot be guessed.
constexpr std::size_t kMatchIdLength = 16;

void sendTo(const std::weak_ptr<LobbyClient>& client, const v1::Envelope& envelope) {
    // lock() returns null if the connection has already closed; then there is no one to tell.
    if (const auto target = client.lock()) {
        std::string bytes;
        envelope.SerializeToString(&bytes);
        target->deliver(std::move(bytes));
    }
}

v1::Envelope errorEnvelope(std::int32_t code, const std::string& message) {
    v1::Envelope envelope;
    envelope.mutable_error()->set_code(code);
    envelope.mutable_error()->set_message(message);
    return envelope;
}

v1::Envelope queueStatus(bool queued, const std::string& mode, const std::string& region) {
    v1::Envelope envelope;
    auto* status = envelope.mutable_queue_status();
    status->set_queued(queued);
    status->set_mode(mode);
    status->set_region(region);
    return envelope;
}

// True if both weak pointers refer to the same connection, even if it has closed.
// owner_before compares the control blocks, which outlive the objects themselves.
bool sameClient(const std::weak_ptr<LobbyClient>& a, const std::weak_ptr<LobbyClient>& b) {
    return !a.owner_before(b) && !b.owner_before(a);
}

// Compares two secrets in constant time. A normal == stops at the first wrong character,
// so an attacker could time responses to guess the secret one character at a time.
bool secretsMatch(std::string_view given, std::string_view expected) {
    if (given.size() != expected.size()) {
        return false;
    }
    unsigned char difference = 0;
    for (std::size_t i = 0; i < given.size(); ++i) {
        difference |= static_cast<unsigned char>(given[i] ^ expected[i]);
    }
    return difference == 0;
}

double averageRating(const std::vector<core::QueuedPlayer>& team) {
    const double total = std::accumulate(
        team.begin(), team.end(), 0.0,
        [](double sum, const core::QueuedPlayer& player) { return sum + player.rating; });
    return team.empty() ? 0.0 : total / static_cast<double>(team.size());
}

}  // namespace

LobbyService::LobbyService(boost::asio::io_context& ioContext, store::AccountStore& store,
                           boost::asio::thread_pool& workers, LobbySettings settings,
                           metrics::Metrics* metrics)
    : m_strand(boost::asio::make_strand(ioContext)),
      m_timer(m_strand),  // The timer's handlers run on the strand, like everything else here.
      m_store(store),
      m_workers(workers),
      m_settings(std::move(settings)),
      m_matchmaker(m_settings.modes, m_settings.regions, m_settings.matchSettings),
      m_metrics(metrics) {}

void LobbyService::start() {
    boost::asio::post(m_strand, [this] {
        if (m_running) {
            return;
        }
        m_running = true;
        scheduleTick();
    });
}

void LobbyService::stop() {
    boost::asio::post(m_strand, [this] {
        m_running = false;
        m_timer.cancel();
    });
}

void LobbyService::scheduleTick() {
    m_timer.expires_after(m_settings.tickInterval);
    m_timer.async_wait([this](boost::system::error_code ec) {
        // operation_aborted means stop() cancelled the timer.
        if (ec || !m_running) {
            return;
        }
        tick();
        scheduleTick();
    });
}

void LobbyService::tick() {
    const core::Clock::time_point now = core::Clock::now();

    // A connection that closed without saying "leave" leaves an expired pointer behind.
    // Remove those players first, so nobody is matched with a player who is gone.
    for (auto it = m_waiting.begin(); it != m_waiting.end();) {
        if (it->second.expired()) {
            m_matchmaker.leave(it->first);
            it = m_waiting.erase(it);
        } else {
            ++it;
        }
    }

    // Forget matches too old to report, so the table cannot grow forever.
    for (auto it = m_matches.begin(); it != m_matches.end();) {
        if (now - it->second.createdAt > m_settings.resultWindow) {
            it = m_matches.erase(it);
        } else {
            ++it;
        }
    }

    for (core::Match& match : m_matchmaker.findMatches(now)) {
        const std::string matchId = auth::newSessionToken().substr(0, kMatchIdLength);
        announce(matchId, match);
        if (m_metrics != nullptr) {
            m_metrics->matchCreated(match.mode);
            for (const auto* team : {&match.teamA, &match.teamB}) {
                for (const core::QueuedPlayer& player : *team) {
                    const std::chrono::duration<double> waited = now - player.joinedAt;
                    m_metrics->queueWait(waited.count());
                }
            }
        }
        m_matches.emplace(matchId, MatchRecord{std::move(match), now});
        ++m_matchesCreated;
    }
    if (m_metrics != nullptr) {
        m_metrics->playersQueued(static_cast<double>(m_matchmaker.queuedCount()));
    }
}

void LobbyService::announce(const std::string& matchId, const core::Match& match) {
    // Build the shared part once: both rosters, as every player will see them.
    v1::Envelope envelope;
    auto* found = envelope.mutable_match_found();
    found->set_match_id(matchId);
    found->set_mode(match.mode);
    found->set_region(match.region);
    for (const auto* team : {&match.teamA, &match.teamB}) {
        auto* roster = found->add_teams();
        for (const core::QueuedPlayer& player : *team) {
            auto* info = roster->add_players();
            info->set_username(player.playerId);
            info->set_rating(player.rating);
        }
    }

    // Then tell each player, adding which team they are on.
    const std::vector<core::QueuedPlayer>* teams[] = {&match.teamA, &match.teamB};
    for (int teamIndex = 0; teamIndex < 2; ++teamIndex) {
        for (const core::QueuedPlayer& player : *teams[teamIndex]) {
            const auto waiting = m_waiting.find(player.playerId);
            if (waiting == m_waiting.end()) {
                continue;
            }
            found->set_your_team(teamIndex);
            sendTo(waiting->second, envelope);
            m_waiting.erase(waiting);  // Matched: no longer waiting on this connection.
        }
    }
    spdlog::info("match {} formed: {} in {}, {} players", matchId, match.mode, match.region,
                 match.teamA.size() + match.teamB.size());
}

void LobbyService::join(std::string username, std::string mode, std::string region,
                        std::weak_ptr<LobbyClient> client) {
    // Reading the rating touches Redis, which can be slow, so it runs on a worker thread.
    // Only then is the player handed to the strand.
    boost::asio::post(
        m_workers, [this, username = std::move(username), mode = std::move(mode),
                    region = std::move(region), client = std::move(client)]() mutable {
            int rating = core::kDefaultRating;
            try {
                rating = m_store.findRating(username).value_or(core::kDefaultRating);
            } catch (const std::exception& error) {
                spdlog::error("lobby: could not load rating: {}", error.what());
                sendTo(client, errorEnvelope(kUnavailable, "service temporarily unavailable"));
                return;
            }

            core::QueuedPlayer player{std::move(username), rating, std::move(mode),
                                      std::move(region), core::Clock::now()};
            boost::asio::post(
                m_strand, [this, player = std::move(player), client = std::move(client)]() mutable {
                    doJoin(std::move(player), std::move(client));
                });
        });
}

void LobbyService::doJoin(core::QueuedPlayer player, std::weak_ptr<LobbyClient> client) {
    const std::string username = player.playerId;
    const std::string mode = player.mode;
    const std::string region = player.region;

    // If the player's old connection closed but the regular pass has not cleaned up yet,
    // drop the stale entry, so a reconnecting player can queue again right away.
    const auto existing = m_waiting.find(username);
    if (existing != m_waiting.end() && existing->second.expired()) {
        m_matchmaker.leave(username);
        m_waiting.erase(existing);
    }

    switch (m_matchmaker.join(std::move(player))) {
        case core::JoinResult::Joined:
            m_waiting[username] = client;
            sendTo(client, queueStatus(true, mode, region));
            break;
        case core::JoinResult::AlreadyQueued:
            sendTo(client, errorEnvelope(kConflict, "already in the queue"));
            break;
        case core::JoinResult::UnknownMode:
            sendTo(client, errorEnvelope(kBadRequest, "unknown mode"));
            break;
        case core::JoinResult::UnknownRegion:
            sendTo(client, errorEnvelope(kBadRequest, "unknown region"));
            break;
    }
}

void LobbyService::leave(std::string username, std::weak_ptr<LobbyClient> client) {
    boost::asio::post(m_strand, [this, username = std::move(username), client = std::move(client)] {
        const auto it = m_waiting.find(username);
        if (it != m_waiting.end() && !sameClient(it->second, client) && !it->second.expired()) {
            // Another live connection queued this player; this one cannot cancel that.
            sendTo(client, errorEnvelope(kConflict, "queued from another connection"));
            return;
        }
        if (it != m_waiting.end()) {
            m_matchmaker.leave(username);
            m_waiting.erase(it);
        }
        // Leaving when not queued is fine: the answer is the same, "not queued".
        sendTo(client, queueStatus(false, "", ""));
    });
}

void LobbyService::disconnect(std::string username, std::weak_ptr<LobbyClient> client) {
    boost::asio::post(m_strand, [this, username = std::move(username), client = std::move(client)] {
        const auto it = m_waiting.find(username);
        if (it != m_waiting.end() && (sameClient(it->second, client) || it->second.expired())) {
            m_matchmaker.leave(username);
            m_waiting.erase(it);
        }
    });
}

void LobbyService::reportResult(std::string matchId, int winningTeam, std::string secret,
                                std::weak_ptr<LobbyClient> reporter) {
    boost::asio::post(m_strand, [this, matchId = std::move(matchId), winningTeam,
                                 secret = std::move(secret),
                                 reporter = std::move(reporter)]() mutable {
        if (m_settings.resultSecret.empty() || !secretsMatch(secret, m_settings.resultSecret)) {
            sendTo(reporter, errorEnvelope(kForbidden, "not allowed to report results"));
            return;
        }
        if (winningTeam < -1 || winningTeam > 1) {
            sendTo(reporter,
                   errorEnvelope(kBadRequest, "winning_team must be 0, 1, or -1 for a draw"));
            return;
        }
        const auto it = m_matches.find(matchId);
        if (it == m_matches.end()) {
            sendTo(reporter, errorEnvelope(kNotFound, "unknown or already reported match"));
            return;
        }

        // Remove the record now, on the strand, so a second report of the same match is
        // rejected even if the first one is still being saved.
        core::Match match = std::move(it->second.match);
        m_matches.erase(it);
        ++m_resultsApplied;
        if (m_metrics != nullptr) {
            m_metrics->resultApplied();
        }

        // Team games use team averages: every player on a team gets the same change.
        const double scoreA =
            winningTeam == -1 ? core::kDraw : (winningTeam == 0 ? core::kWin : core::kLoss);
        const double averageA = averageRating(match.teamA);
        const double averageB = averageRating(match.teamB);
        const int deltaA = core::ratingDelta(averageA, averageB, scoreA, m_settings.kFactor);
        const int deltaB = core::ratingDelta(averageB, averageA, 1.0 - scoreA, m_settings.kFactor);

        // Saving to Redis is slow, so it runs on a worker. The change is added to the rating
        // stored now, not to the rating at match time, so results that arrive out of order
        // or at the same time are all applied (see AccountStore::adjustRating).
        boost::asio::post(m_workers, [this, matchId, match = std::move(match), deltaA, deltaB,
                                      reporter = std::move(reporter)] {
            v1::Envelope envelope;
            auto* ack = envelope.mutable_result_ack();
            ack->set_match_id(matchId);
            try {
                for (const auto& [team, delta] :
                     {std::pair{&match.teamA, deltaA}, std::pair{&match.teamB, deltaB}}) {
                    for (const core::QueuedPlayer& player : *team) {
                        const int newRating =
                            m_store.adjustRating(player.playerId, delta, core::kDefaultRating);
                        auto* info = ack->add_new_ratings();
                        info->set_username(player.playerId);
                        info->set_rating(newRating);
                    }
                }
            } catch (const std::exception& error) {
                spdlog::error("lobby: could not save ratings for match {}: {}", matchId,
                              error.what());
                sendTo(reporter, errorEnvelope(kUnavailable, "service temporarily unavailable"));
                return;
            }
            spdlog::info("match {} result applied: team A {:+d}, team B {:+d}", matchId, deltaA,
                         deltaB);
            sendTo(reporter, envelope);
        });
    });
}

void LobbyService::stats(std::function<void(LobbyStats)> callback) {
    boost::asio::post(m_strand, [this, callback = std::move(callback)] {
        LobbyStats stats;
        stats.playersQueued = m_matchmaker.queuedCount();
        stats.matchesAwaitingResult = m_matches.size();
        stats.matchesCreated = m_matchesCreated;
        stats.resultsApplied = m_resultsApplied;
        callback(stats);
    });
}

}  // namespace matchforge::lobby

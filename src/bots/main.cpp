// main.cpp for the bots: a load simulator. It starts many fake players, each on its own
// thread. Every bot registers, joins a queue, waits for a match, and repeats. At the end it
// prints how long matching took, so you can see how the server behaves under load.
//
// Example: matchforge_bots --bots 100 --rounds 5 --mode duel --region eu
#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdio>
#include <exception>
#include <map>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "bots/BotClient.h"
#include "bots/BotOptions.h"
#include "bots/BotReport.h"
#include "core/Version.h"
#include "matchforge.pb.h"

namespace {

using matchforge::bots::BotClient;
using matchforge::bots::BotOptions;
using matchforge::v1::Envelope;

// All bots share one password. They are throwaway test accounts.
constexpr const char* kBotPassword = "bot-password-123";

// Results collected from every bot thread. All access goes through the mutex.
struct RunResults {
    std::mutex mutex;
    std::vector<double> waitMilliseconds;  // join -> MatchFound, per match
    int matchesJoined = 0;
    int resultsReported = 0;
    int unmatched = 0;                  // gave up waiting for a match
    std::map<std::string, int> errors;  // error message -> how many times

    void addWait(double ms) {
        std::lock_guard lock(mutex);
        waitMilliseconds.push_back(ms);
        ++matchesJoined;
    }
    void addError(const std::string& what) {
        std::lock_guard lock(mutex);
        ++errors[what];
    }
};

// Describes a failed wait: the server's error message, or "no reply" if nothing came back.
std::string describeFailure(const std::string& step, const Envelope& reply) {
    if (reply.payload_case() == Envelope::kError) {
        return step + ": " + std::to_string(reply.error().code()) + " " + reply.error().message();
    }
    return step + ": no reply (timeout or disconnect)";
}

// One bot's whole life: connect, register, then play `rounds` matches.
void runBot(int index, const BotOptions& options, const std::string& runId, RunResults& results) {
    // Unique per run, so bots never collide with accounts from an earlier run.
    const std::string name = "bot_" + runId + "_" + std::to_string(index);
    std::mt19937 random(std::random_device{}());

    try {
        BotClient client(options.host, options.port, options.timeoutSeconds);
        Envelope reply;

        Envelope registerRequest;
        registerRequest.mutable_register_()->set_username(name);
        registerRequest.mutable_register_()->set_password(kBotPassword);
        client.send(registerRequest);
        if (!client.waitFor(Envelope::kAuth, reply)) {
            results.addError(describeFailure("register", reply));
            return;
        }

        for (int round = 0; round < options.rounds; ++round) {
            Envelope join;
            join.mutable_join_queue()->set_mode(options.mode);
            join.mutable_join_queue()->set_region(options.region);
            const auto joinedAt = std::chrono::steady_clock::now();
            client.send(join);

            if (!client.waitFor(Envelope::kQueueStatus, reply)) {
                results.addError(describeFailure("join", reply));
                return;
            }
            if (!client.waitFor(Envelope::kMatchFound, reply)) {
                std::lock_guard lock(results.mutex);
                ++results.unmatched;  // Usually an odd number of bots: someone is left over.
                return;
            }
            const std::chrono::duration<double, std::milli> waited =
                std::chrono::steady_clock::now() - joinedAt;
            results.addWait(waited.count());

            // One bot per match plays the game server: the first player listed on team 0.
            // It reports a random winner, so ratings spread out over a long run.
            const auto& found = reply.match_found();
            const bool isReporter = found.teams_size() == 2 && found.teams(0).players_size() > 0 &&
                                    found.teams(0).players(0).username() == name;
            if (!options.resultSecret.empty() && isReporter) {
                Envelope report;
                report.mutable_report_result()->set_match_id(found.match_id());
                report.mutable_report_result()->set_winning_team(static_cast<int>(random() % 2));
                report.mutable_report_result()->set_result_secret(options.resultSecret);
                client.send(report);
                if (client.waitFor(Envelope::kResultAck, reply)) {
                    std::lock_guard lock(results.mutex);
                    ++results.resultsReported;
                } else {
                    results.addError(describeFailure("report", reply));
                }
            }
        }
    } catch (const std::exception& error) {
        results.addError(std::string("connection: ") + error.what());
    }
}

// Six random hex characters, used to make this run's bot names unique.
std::string makeRunId() {
    std::random_device device;
    const char* digits = "0123456789abcdef";
    std::string id;
    for (int i = 0; i < 6; ++i) {
        id.push_back(digits[device() % 16]);
    }
    return id;
}

}  // namespace

int main(int argc, char* argv[]) {
    const std::vector<std::string> args(argv + 1, argv + argc);
    std::string error;
    const auto options = matchforge::bots::parseBotOptions(args, error);
    if (!options) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return error == matchforge::bots::botUsage() ? 0 : 2;  // --help is not an error
    }

    spdlog::info("matchforge bots {}: {} bots x {} rounds, {} in {}, server {}:{}",
                 matchforge::core::version(), options->bots, options->rounds, options->mode,
                 options->region, options->host, options->port);

    RunResults results;
    const std::string runId = makeRunId();
    const auto started = std::chrono::steady_clock::now();

    std::vector<std::thread> bots;
    bots.reserve(static_cast<std::size_t>(options->bots));
    for (int i = 0; i < options->bots; ++i) {
        bots.emplace_back(runBot, i, std::cref(*options), std::cref(runId), std::ref(results));
    }
    for (auto& bot : bots) {
        bot.join();
    }

    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - started;
    const auto summary = matchforge::bots::summarize(results.waitMilliseconds);
    int errorCount = 0;
    for (const auto& [message, count] : results.errors) {
        spdlog::warn("{} x {}", count, message);
        errorCount += count;
    }

    spdlog::info("matches joined: {}, results reported: {}, unmatched: {}, errors: {}",
                 results.matchesJoined, results.resultsReported, results.unmatched, errorCount);
    spdlog::info("time to match (ms): avg {:.1f}, p50 {:.1f}, p95 {:.1f}, max {:.1f}",
                 summary.average, summary.p50, summary.p95, summary.max);
    spdlog::info("wall time {:.2f} s, {:.1f} matches joined per second", elapsed.count(),
                 elapsed.count() > 0 ? results.matchesJoined / elapsed.count() : 0.0);

    // A non-zero exit code lets scripts and CI treat a bad run as a failure.
    return (errorCount == 0 && results.unmatched == 0) ? 0 : 1;
}

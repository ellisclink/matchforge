// Metrics.h: numbers that describe what the server is doing, for Prometheus to collect.
//
// Prometheus "scrapes" (fetches) http://server:9100/metrics every few seconds and stores
// the values over time; Grafana draws graphs from them. Three kinds of metric are used:
//   counter    only goes up (total connections, total matches). Graphs show its rate.
//   gauge      goes up and down (connections open right now, players queued right now).
//   histogram  counts values into buckets (queue wait times), so percentiles can be drawn.
//
// Every method is thread-safe (prometheus-cpp uses atomics), so sessions on any thread,
// the lobby strand and worker threads can all record without extra locking.
//
// Components take a `Metrics*` that may be null: tests and tools pass nothing and pay nothing.
#pragma once

#include <memory>
#include <string>

namespace prometheus {
class Counter;
class Gauge;
class Histogram;
class Registry;
template <typename T>
class Family;
}  // namespace prometheus

namespace matchforge::metrics {

class Metrics {
public:
    Metrics();
    ~Metrics();

    Metrics(const Metrics&) = delete;
    Metrics& operator=(const Metrics&) = delete;

    // The registry holding every metric; the HTTP exposer serves it.
    std::shared_ptr<prometheus::Registry> registry() const { return m_registry; }

    // Connections
    void connectionOpened();
    void connectionClosed();

    // Messages
    void messageReceived();
    void messageRateLimited();

    // Accounts. result is "success", "failure" or "locked".
    void loginAttempt(const std::string& result);
    void registration(const std::string& result);

    // Matchmaking
    void playersQueued(double count);  // set after every matching pass
    void matchCreated(const std::string& mode);
    void queueWait(double seconds);  // one observation per matched player
    void resultApplied();

private:
    std::shared_ptr<prometheus::Registry> m_registry;

    prometheus::Counter& m_connectionsTotal;
    prometheus::Gauge& m_connectionsOpen;
    prometheus::Counter& m_messagesReceived;
    prometheus::Counter& m_messagesRateLimited;
    prometheus::Family<prometheus::Counter>& m_logins;
    prometheus::Family<prometheus::Counter>& m_registrations;
    prometheus::Gauge& m_playersQueued;
    prometheus::Family<prometheus::Counter>& m_matchesCreated;
    prometheus::Histogram& m_queueWaitSeconds;
    prometheus::Counter& m_resultsApplied;
};

}  // namespace matchforge::metrics

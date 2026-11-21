// Metrics.cpp: creates every metric once and records values into them.
#include "metrics/Metrics.h"

#include <prometheus/counter.h>
#include <prometheus/family.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/registry.h>

namespace matchforge::metrics {

namespace {

// Builds a family and returns its single unlabelled metric. Names follow Prometheus
// conventions: a "matchforge_" prefix, snake_case, and "_total" for counters.
prometheus::Counter& counter(prometheus::Registry& registry, const std::string& name,
                             const std::string& help) {
    return prometheus::BuildCounter().Name(name).Help(help).Register(registry).Add({});
}

prometheus::Gauge& gauge(prometheus::Registry& registry, const std::string& name,
                         const std::string& help) {
    return prometheus::BuildGauge().Name(name).Help(help).Register(registry).Add({});
}

// Bucket edges for queue wait, in seconds. Most matches should land in the first few;
// the later ones show players who waited a long time for a wide rating window.
const prometheus::Histogram::BucketBoundaries kWaitBuckets{0.1, 0.25, 0.5, 1,  2,
                                                           5,   10,   30,  60, 120};

}  // namespace

Metrics::Metrics()
    : m_registry(std::make_shared<prometheus::Registry>()),
      m_connectionsTotal(counter(*m_registry, "matchforge_connections_total",
                                 "TCP connections accepted since the server started")),
      m_connectionsOpen(
          gauge(*m_registry, "matchforge_connections_open", "TCP connections open now")),
      m_messagesReceived(counter(*m_registry, "matchforge_messages_received_total",
                                 "Messages received from clients")),
      m_messagesRateLimited(counter(*m_registry, "matchforge_messages_rate_limited_total",
                                    "Messages refused because a client sent too fast")),
      m_logins(prometheus::BuildCounter()
                   .Name("matchforge_logins_total")
                   .Help("Login attempts by result (success, failure, locked)")
                   .Register(*m_registry)),
      m_registrations(prometheus::BuildCounter()
                          .Name("matchforge_registrations_total")
                          .Help("Registration attempts by result (success, failure)")
                          .Register(*m_registry)),
      m_playersQueued(
          gauge(*m_registry, "matchforge_players_queued", "Players waiting for a match now")),
      m_matchesCreated(prometheus::BuildCounter()
                           .Name("matchforge_matches_created_total")
                           .Help("Matches formed, by mode")
                           .Register(*m_registry)),
      m_queueWaitSeconds(prometheus::BuildHistogram()
                             .Name("matchforge_queue_wait_seconds")
                             .Help("Time from joining the queue to being matched")
                             .Register(*m_registry)
                             .Add({}, kWaitBuckets)),
      m_resultsApplied(counter(*m_registry, "matchforge_results_applied_total",
                               "Match results accepted and applied to ratings")) {}

Metrics::~Metrics() = default;

void Metrics::connectionOpened() {
    m_connectionsTotal.Increment();
    m_connectionsOpen.Increment();
}

void Metrics::connectionClosed() { m_connectionsOpen.Decrement(); }

void Metrics::messageReceived() { m_messagesReceived.Increment(); }

void Metrics::messageRateLimited() { m_messagesRateLimited.Increment(); }

void Metrics::loginAttempt(const std::string& result) {
    // Add() with the same labels returns the same counter, so this is cheap after the first call.
    m_logins.Add({{"result", result}}).Increment();
}

void Metrics::registration(const std::string& result) {
    m_registrations.Add({{"result", result}}).Increment();
}

void Metrics::playersQueued(double count) { m_playersQueued.Set(count); }

void Metrics::matchCreated(const std::string& mode) {
    m_matchesCreated.Add({{"mode", mode}}).Increment();
}

void Metrics::queueWait(double seconds) { m_queueWaitSeconds.Observe(seconds); }

void Metrics::resultApplied() { m_resultsApplied.Increment(); }

}  // namespace matchforge::metrics

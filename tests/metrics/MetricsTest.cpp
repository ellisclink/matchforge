// MetricsTest.cpp: metric values recorded by the server. Values are read from the registry's
// collected data (not the text output), so the test does not depend on number formatting.
#include <gtest/gtest.h>
#include <prometheus/metric_family.h>
#include <prometheus/registry.h>
#include <prometheus/text_serializer.h>

#include <string>

#include "matchforge.pb.h"
#include "metrics/Metrics.h"
#include "net/TestSupport.h"

using matchforge::metrics::Metrics;
using matchforge::test::ServerFixture;
using matchforge::test::TestClient;
using matchforge::v1::Envelope;

namespace {

// Returns the value of metric `name` whose label `labelName` equals `labelValue` (pass empty
// strings for an unlabelled metric). Counters and gauges return their value; histograms
// return their sample count. Returns -1 if no such metric exists.
double valueOf(Metrics& metrics, const std::string& name, const std::string& labelName = "",
               const std::string& labelValue = "") {
    for (const prometheus::MetricFamily& family : metrics.registry()->Collect()) {
        if (family.name != name) {
            continue;
        }
        for (const prometheus::ClientMetric& metric : family.metric) {
            bool labelMatches = labelName.empty();
            for (const auto& label : metric.label) {
                labelMatches =
                    labelMatches || (label.name == labelName && label.value == labelValue);
            }
            if (!labelMatches) {
                continue;
            }
            switch (family.type) {
                case prometheus::MetricType::Counter:
                    return metric.counter.value;
                case prometheus::MetricType::Gauge:
                    return metric.gauge.value;
                case prometheus::MetricType::Histogram:
                    return static_cast<double>(metric.histogram.sample_count);
                default:
                    return -1;
            }
        }
    }
    return -1;
}

}  // namespace

TEST(MetricsTest, CountersGaugesAndHistogramsRecordValues) {
    Metrics metrics;

    metrics.connectionOpened();
    metrics.connectionOpened();
    metrics.connectionClosed();
    metrics.loginAttempt("success");
    metrics.loginAttempt("failure");
    metrics.loginAttempt("failure");
    metrics.matchCreated("duel");
    metrics.playersQueued(7);
    metrics.queueWait(0.3);
    metrics.queueWait(4.0);

    EXPECT_EQ(valueOf(metrics, "matchforge_connections_total"), 2);
    EXPECT_EQ(valueOf(metrics, "matchforge_connections_open"), 1);
    EXPECT_EQ(valueOf(metrics, "matchforge_logins_total", "result", "success"), 1);
    EXPECT_EQ(valueOf(metrics, "matchforge_logins_total", "result", "failure"), 2);
    EXPECT_EQ(valueOf(metrics, "matchforge_matches_created_total", "mode", "duel"), 1);
    EXPECT_EQ(valueOf(metrics, "matchforge_players_queued"), 7);
    EXPECT_EQ(valueOf(metrics, "matchforge_queue_wait_seconds"), 2);
}

TEST(MetricsTest, TheTextFormatNamesEveryMetric) {
    Metrics metrics;
    metrics.messageReceived();

    // This is the text Prometheus reads from /metrics.
    const std::string text = prometheus::TextSerializer().Serialize(metrics.registry()->Collect());

    EXPECT_NE(text.find("# HELP matchforge_messages_received_total"), std::string::npos);
    EXPECT_NE(text.find("# TYPE matchforge_queue_wait_seconds histogram"), std::string::npos);
}

// The real server records metrics as clients use it.
TEST_F(ServerFixture, ServerRecordsLoginsMatchesAndConnections) {
    Metrics& metrics = m_testServer.metrics();
    TestClient alice(port());
    TestClient bob(port());

    alice.registerAs("metricalice");
    bob.registerAs("metricbob");

    Envelope wrongLogin;
    wrongLogin.mutable_login()->set_username("metricalice");
    wrongLogin.mutable_login()->set_password("wrong horse");
    alice.send(wrongLogin);
    alice.expect(Envelope::kError);

    alice.joinQueue("duel", "eu");
    alice.expect(Envelope::kQueueStatus);
    bob.joinQueue("duel", "eu");
    bob.expect(Envelope::kQueueStatus);
    alice.expect(Envelope::kMatchFound);
    bob.expect(Envelope::kMatchFound);

    EXPECT_GE(valueOf(metrics, "matchforge_connections_total"), 2);
    EXPECT_EQ(valueOf(metrics, "matchforge_registrations_total", "result", "success"), 2);
    EXPECT_EQ(valueOf(metrics, "matchforge_logins_total", "result", "failure"), 1);
    EXPECT_EQ(valueOf(metrics, "matchforge_matches_created_total", "mode", "duel"), 1);
    EXPECT_EQ(valueOf(metrics, "matchforge_queue_wait_seconds"), 2);  // one sample per player
    // Alice sent register, login and join; Bob sent register and join.
    EXPECT_EQ(valueOf(metrics, "matchforge_messages_received_total"), 5);
}

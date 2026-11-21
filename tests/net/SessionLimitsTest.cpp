// SessionLimitsTest.cpp: the per-connection protections. Each test builds its own server
// with tight limits, so the effects show up quickly.
#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>

#include "matchforge.pb.h"
#include "net/Framing.h"
#include "net/TestSupport.h"

using matchforge::net::SessionLimits;
using matchforge::test::defaultTestLimits;
using matchforge::test::TestClient;
using matchforge::test::TestServer;
using matchforge::v1::Envelope;
using namespace std::chrono_literals;

namespace {

Envelope heartbeat() {
    Envelope envelope;
    envelope.mutable_heartbeat()->set_sent_at_unix_ms(1);
    return envelope;
}

// Several heartbeats in one write, so they arrive together and hit the limiter at once.
std::string heartbeats(int count) {
    std::string bytes;
    heartbeat().SerializeToString(&bytes);
    const std::string frame = matchforge::net::encodeFrame(bytes);
    std::string all;
    for (int i = 0; i < count; ++i) {
        all += frame;
    }
    return all;
}

}  // namespace

TEST(SessionLimitsTest, AnIdleConnectionIsClosed) {
    SessionLimits limits = defaultTestLimits();
    limits.idleTimeout = 1s;
    TestServer server(limits);
    TestClient client(server.port());

    const auto start = std::chrono::steady_clock::now();
    Envelope unused;
    EXPECT_FALSE(client.receive(unused));  // The server closes the silent connection.
    const auto waited = std::chrono::steady_clock::now() - start;

    EXPECT_GE(waited, 900ms);  // Not before the timeout...
    EXPECT_LT(waited, 8s);     // ...and well before the client's own 10-second read timeout.
}

TEST(SessionLimitsTest, HeartbeatsKeepAConnectionAlive) {
    SessionLimits limits = defaultTestLimits();
    limits.idleTimeout = 1s;
    TestServer server(limits);
    TestClient client(server.port());

    // 2.5 seconds of activity, more than twice the idle timeout.
    for (int i = 0; i < 6; ++i) {
        client.send(heartbeat());
        client.expect(Envelope::kHeartbeat);
        std::this_thread::sleep_for(400ms);
    }
    client.send(heartbeat());
    client.expect(Envelope::kHeartbeat);  // Still connected.
}

TEST(SessionLimitsTest, MessagesAboveTheBurstGetTooManyRequests) {
    SessionLimits limits = defaultTestLimits();
    limits.messagesPerSecond = 1;
    limits.messageBurst = 3;
    TestServer server(limits);
    TestClient client(server.port());

    client.sendRaw(heartbeats(6));

    // The first 3 fit in the bucket; the other 3 are refused.
    for (int i = 0; i < 3; ++i) {
        client.expect(Envelope::kHeartbeat);
    }
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(client.expect(Envelope::kError).error().code(), 429);
    }
}

TEST(SessionLimitsTest, AClientThatKeepsFloodingIsDisconnected) {
    SessionLimits limits = defaultTestLimits();
    limits.messagesPerSecond = 0.1;
    limits.messageBurst = 1;
    TestServer server(limits);
    TestClient client(server.port());

    client.sendRaw(heartbeats(80));

    // One heartbeat is allowed, then 50 refusals, then the server hangs up.
    int heartbeatsSeen = 0;
    int refusals = 0;
    Envelope reply;
    while (client.receive(reply)) {
        if (reply.payload_case() == Envelope::kHeartbeat) {
            ++heartbeatsSeen;
        } else if (reply.payload_case() == Envelope::kError && reply.error().code() == 429) {
            ++refusals;
        }
    }
    EXPECT_EQ(heartbeatsSeen, 1);
    EXPECT_EQ(refusals, 50);
}

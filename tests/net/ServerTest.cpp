// ServerTest.cpp: integration tests. A real Server listens on loopback, and a
// blocking client socket talks to it, so the whole path is exercised: accept,
// framing, protobuf, auth, reply.
#include <gtest/gtest.h>

#include <string>

#include "matchforge.pb.h"
#include "net/Framing.h"
#include "net/TestSupport.h"

using matchforge::test::ServerFixture;
using matchforge::test::TestClient;
using matchforge::v1::Envelope;

// A heartbeat from the client should come back as a heartbeat from the server, with a clock value.
TEST_F(ServerFixture, RepliesToHeartbeatWithAHeartbeat) {
    TestClient client(port());

    Envelope heartbeat;
    heartbeat.mutable_heartbeat()->set_sent_at_unix_ms(42);
    client.send(heartbeat);

    const Envelope reply = client.expect(Envelope::kHeartbeat);
    EXPECT_GT(reply.heartbeat().sent_at_unix_ms(), 0u);  // The server's clock is set.
}

// Garbage bytes that are not a valid Envelope get an ErrorResponse, then the connection closes.
TEST_F(ServerFixture, MalformedMessageGetsAnErrorThenDisconnect) {
    TestClient client(port());

    // 0xFF is not a valid protobuf tag, so this payload fails to parse.
    client.sendRaw(matchforge::net::encodeFrame("\xff\xff\xff"));

    const Envelope reply = client.expect(Envelope::kError);
    EXPECT_EQ(reply.error().code(), 400);

    // After the error, the server closes the connection, so the next read reports end of file.
    Envelope unused;
    EXPECT_FALSE(client.receive(unused));
}

// A frame announcing more than the limit makes the server drop the connection without a reply.
TEST_F(ServerFixture, OversizedFrameDisconnectsWithoutReply) {
    TestClient client(port());

    // Send only a header claiming 100000 bytes, far above the 1024-byte limit.
    client.sendRaw(std::string("\x00\x01\x86\xa0", 4));

    Envelope unused;
    EXPECT_FALSE(client.receive(unused));
}

// A server-to-client message sent by a client is a bad request, but the connection stays open.
TEST_F(ServerFixture, ServerOnlyMessagesFromAClientAreRejected) {
    TestClient client(port());

    Envelope bogus;
    bogus.mutable_match_found()->set_match_id("fake");
    client.send(bogus);
    EXPECT_EQ(client.expect(Envelope::kError).error().code(), 400);

    Envelope heartbeat;
    heartbeat.mutable_heartbeat();
    client.send(heartbeat);
    client.expect(Envelope::kHeartbeat);  // Still connected.
}

// Registering over the connection returns a session token and the starting rating.
TEST_F(ServerFixture, RegisterReturnsASessionTokenAndStartingRating) {
    TestClient client(port());

    const auto auth = client.registerAs("dave");

    EXPECT_EQ(auth.username(), "dave");
    EXPECT_EQ(auth.session_token().size(), 43u);
    EXPECT_EQ(auth.rating(), 1500);
}

// The same name cannot be registered twice. The second attempt gets a 409 error on the same
// connection.
TEST_F(ServerFixture, DuplicateRegisterGetsAConflictError) {
    TestClient client(port());
    client.registerAs("erin");

    Envelope again;
    again.mutable_register_()->set_username("erin");
    again.mutable_register_()->set_password("another password");
    client.send(again);

    EXPECT_EQ(client.expect(Envelope::kError).error().code(), 409);
}

// A client can log in to an account it registered, and the connection stays open afterwards.
TEST_F(ServerFixture, LoginAfterRegisterReturnsASessionAndKeepsTheConnection) {
    TestClient client(port());
    client.registerAs("frank");

    const auto auth = client.loginAs("frank");
    EXPECT_EQ(auth.username(), "frank");

    // Replies come back in the order requests were sent, and the connection is still usable.
    Envelope heartbeat;
    heartbeat.mutable_heartbeat()->set_sent_at_unix_ms(1);
    client.send(heartbeat);
    client.expect(Envelope::kHeartbeat);
}

// A wrong password gets a 401 error, and the message does not reveal whether the account exists.
TEST_F(ServerFixture, WrongPasswordGetsAnAuthError) {
    TestClient client(port());
    client.registerAs("gina");

    Envelope login;
    login.mutable_login()->set_username("gina");
    login.mutable_login()->set_password("wrong horse");
    client.send(login);

    const Envelope reply = client.expect(Envelope::kError);
    EXPECT_EQ(reply.error().code(), 401);
    EXPECT_EQ(reply.error().message(), "invalid username or password");
}

// BotClientTest.cpp: the bot's client against a real test server, to prove the bots speak
// the same protocol as the server.
#include <gtest/gtest.h>

#include "bots/BotClient.h"
#include "matchforge.pb.h"
#include "net/TestSupport.h"

using matchforge::bots::BotClient;
using matchforge::test::ServerFixture;
using matchforge::v1::Envelope;

TEST_F(ServerFixture, BotClientsRegisterQueueAndGetMatched) {
    BotClient first("127.0.0.1", port(), 10);
    BotClient second("127.0.0.1", port(), 10);
    Envelope reply;

    Envelope registerFirst;
    registerFirst.mutable_register_()->set_username("botone");
    registerFirst.mutable_register_()->set_password("bot-password-123");
    first.send(registerFirst);
    ASSERT_TRUE(first.waitFor(Envelope::kAuth, reply));

    Envelope registerSecond = registerFirst;
    registerSecond.mutable_register_()->set_username("bottwo");
    second.send(registerSecond);
    ASSERT_TRUE(second.waitFor(Envelope::kAuth, reply));

    Envelope join;
    join.mutable_join_queue()->set_mode("duel");
    join.mutable_join_queue()->set_region("eu");
    first.send(join);
    second.send(join);

    // waitFor skips the QueueStatus that arrives first and returns the MatchFound.
    Envelope firstFound;
    Envelope secondFound;
    ASSERT_TRUE(first.waitFor(Envelope::kMatchFound, firstFound));
    ASSERT_TRUE(second.waitFor(Envelope::kMatchFound, secondFound));
    EXPECT_EQ(firstFound.match_found().match_id(), secondFound.match_found().match_id());
}

TEST_F(ServerFixture, BotClientWaitForStopsAtAnError) {
    BotClient client("127.0.0.1", port(), 10);

    Envelope join;  // Not logged in, so the server answers with ErrorResponse 401.
    join.mutable_join_queue()->set_mode("duel");
    join.mutable_join_queue()->set_region("eu");
    client.send(join);

    Envelope reply;
    EXPECT_FALSE(client.waitFor(Envelope::kQueueStatus, reply));
    ASSERT_EQ(reply.payload_case(), Envelope::kError);
    EXPECT_EQ(reply.error().code(), 401);
}

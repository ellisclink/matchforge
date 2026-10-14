// LobbyTest.cpp: the matchmaking queue end to end. Real clients register, join queues,
// get matched, and a game server reports results, all over loopback TCP.
#include <gtest/gtest.h>

#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "matchforge.pb.h"
#include "net/TestSupport.h"

using matchforge::test::ServerFixture;
using matchforge::test::TestClient;
using matchforge::v1::Envelope;

namespace {

class LobbyTest : public ServerFixture {
protected:
    // Registers two players and queues them in the same mode and region, then returns the
    // MatchFound each one received.
    std::pair<Envelope, Envelope> matchTwoPlayers(TestClient& a, TestClient& b,
                                                  const std::string& nameA,
                                                  const std::string& nameB) {
        a.registerAs(nameA);
        b.registerAs(nameB);
        a.joinQueue("duel", "eu");
        EXPECT_TRUE(a.expect(Envelope::kQueueStatus).queue_status().queued());
        b.joinQueue("duel", "eu");
        EXPECT_TRUE(b.expect(Envelope::kQueueStatus).queue_status().queued());
        Envelope foundA = a.expect(Envelope::kMatchFound);
        Envelope foundB = b.expect(Envelope::kMatchFound);
        return {foundA, foundB};
    }
};

}  // namespace

TEST_F(LobbyTest, JoiningRequiresLogin) {
    TestClient client(port());

    client.joinQueue("duel", "eu");

    EXPECT_EQ(client.expect(Envelope::kError).error().code(), 401);
}

TEST_F(LobbyTest, TwoPlayersInTheSameQueueAreMatched) {
    TestClient alice(port());
    TestClient bob(port());

    const auto [foundA, foundB] = matchTwoPlayers(alice, bob, "alice", "bob");

    // Both see the same match, with one player per team, on opposite teams.
    EXPECT_EQ(foundA.match_found().match_id(), foundB.match_found().match_id());
    EXPECT_EQ(foundA.match_found().match_id().size(), 16u);
    EXPECT_EQ(foundA.match_found().mode(), "duel");
    EXPECT_EQ(foundA.match_found().region(), "eu");
    ASSERT_EQ(foundA.match_found().teams_size(), 2);
    EXPECT_EQ(foundA.match_found().teams(0).players_size(), 1);
    EXPECT_EQ(foundA.match_found().teams(1).players_size(), 1);
    EXPECT_NE(foundA.match_found().your_team(), foundB.match_found().your_team());

    // Each player's own name is on the team they were told they are on.
    const auto& teamOfA = foundA.match_found().teams(foundA.match_found().your_team());
    EXPECT_EQ(teamOfA.players(0).username(), "alice");
    EXPECT_EQ(teamOfA.players(0).rating(), 1500);
}

TEST_F(LobbyTest, UnknownModeAndRegionAreRejected) {
    TestClient client(port());
    client.registerAs("hank");

    client.joinQueue("battle-royale", "eu");
    EXPECT_EQ(client.expect(Envelope::kError).error().code(), 400);

    client.joinQueue("duel", "moon");
    EXPECT_EQ(client.expect(Envelope::kError).error().code(), 400);
}

TEST_F(LobbyTest, JoiningTwiceIsAConflict) {
    TestClient client(port());
    client.registerAs("ivy");

    client.joinQueue("duel", "eu");
    client.expect(Envelope::kQueueStatus);
    client.joinQueue("duel", "eu");

    EXPECT_EQ(client.expect(Envelope::kError).error().code(), 409);
}

TEST_F(LobbyTest, LeavingTheQueueAndJoiningAgain) {
    TestClient client(port());
    client.registerAs("jack");

    client.joinQueue("duel", "na");
    EXPECT_TRUE(client.expect(Envelope::kQueueStatus).queue_status().queued());

    client.leaveQueue();
    EXPECT_FALSE(client.expect(Envelope::kQueueStatus).queue_status().queued());

    client.joinQueue("duel", "na");
    EXPECT_TRUE(client.expect(Envelope::kQueueStatus).queue_status().queued());
}

TEST_F(LobbyTest, APlayerWhoDisconnectsIsNotMatched) {
    {
        TestClient ghost(port());
        ghost.registerAs("ghost");
        ghost.joinQueue("duel", "eu");
        ghost.expect(Envelope::kQueueStatus);
        ghost.close();  // Leaves without saying "leave".
    }

    TestClient bob(port());
    TestClient carol(port());
    const auto [foundB, foundC] = matchTwoPlayers(bob, carol, "bob2", "carol2");

    // Bob and Carol play each other; the ghost is not in the match.
    for (const auto& team : foundB.match_found().teams()) {
        for (const auto& player : team.players()) {
            EXPECT_NE(player.username(), "ghost");
        }
    }
    EXPECT_EQ(foundB.match_found().match_id(), foundC.match_found().match_id());
}

TEST_F(LobbyTest, ReportedResultUpdatesBothRatings) {
    TestClient alice(port());
    TestClient bob(port());
    const auto [foundA, foundB] = matchTwoPlayers(alice, bob, "alice3", "bob3");
    const std::string matchId = foundA.match_found().match_id();
    const int aliceTeam = foundA.match_found().your_team();

    // A game server reports that Alice's team won. It does not log in; it proves itself
    // with the shared secret.
    TestClient gameServer(port());
    gameServer.reportResult(matchId, aliceTeam, kResultSecret);
    const Envelope ack = gameServer.expect(Envelope::kResultAck);

    std::map<std::string, int> newRatings;
    for (const auto& player : ack.result_ack().new_ratings()) {
        newRatings[player.username()] = player.rating();
    }
    // Equal ratings, K = 32: the winner gains 16 and the loser loses 16.
    EXPECT_EQ(newRatings["alice3"], 1516);
    EXPECT_EQ(newRatings["bob3"], 1484);
    EXPECT_EQ(m_store.findRating("alice3"), 1516);
    EXPECT_EQ(m_store.findRating("bob3"), 1484);

    // The same match cannot be reported twice.
    gameServer.reportResult(matchId, aliceTeam, kResultSecret);
    EXPECT_EQ(gameServer.expect(Envelope::kError).error().code(), 404);

    // Logging in again shows the new rating.
    TestClient aliceAgain(port());
    EXPECT_EQ(aliceAgain.loginAs("alice3").rating(), 1516);
}

TEST_F(LobbyTest, AWrongSecretCannotChangeRatings) {
    TestClient alice(port());
    TestClient bob(port());
    const auto [foundA, foundB] = matchTwoPlayers(alice, bob, "alice4", "bob4");

    TestClient cheater(port());
    cheater.reportResult(foundA.match_found().match_id(), 0, "guessed-secret");

    EXPECT_EQ(cheater.expect(Envelope::kError).error().code(), 403);
    EXPECT_FALSE(m_store.findRating("alice4").has_value());  // Nothing was saved.
}

TEST_F(LobbyTest, ADrawLeavesEqualRatingsUnchanged) {
    TestClient alice(port());
    TestClient bob(port());
    const auto [foundA, foundB] = matchTwoPlayers(alice, bob, "alice5", "bob5");

    TestClient gameServer(port());
    gameServer.reportResult(foundA.match_found().match_id(), -1, kResultSecret);
    gameServer.expect(Envelope::kResultAck);

    EXPECT_EQ(m_store.findRating("alice5"), 1500);
    EXPECT_EQ(m_store.findRating("bob5"), 1500);
}

// Concurrency (Milestone 5): many clients on separate threads register and queue at the same
// time. Every player must be matched exactly once, and every match must have two players.
TEST_F(LobbyTest, ManyConcurrentPlayersAreEachMatchedExactlyOnce) {
    constexpr int kPlayers = 16;
    std::mutex resultsMutex;
    std::map<std::string, std::vector<std::string>> playersByMatch;  // match id -> usernames

    std::vector<std::thread> players;
    for (int i = 0; i < kPlayers; ++i) {
        players.emplace_back([&, i] {
            TestClient client(port());
            const std::string name = "load" + std::to_string(i);
            client.registerAs(name);
            client.joinQueue("duel", "eu");
            client.expect(Envelope::kQueueStatus);
            const Envelope found = client.expect(Envelope::kMatchFound);

            std::lock_guard lock(resultsMutex);
            playersByMatch[found.match_found().match_id()].push_back(name);
        });
    }
    for (auto& player : players) {
        player.join();
    }

    EXPECT_EQ(playersByMatch.size(), static_cast<std::size_t>(kPlayers / 2));
    std::set<std::string> everyone;
    for (const auto& [matchId, names] : playersByMatch) {
        EXPECT_EQ(names.size(), 2u) << "match " << matchId;
        everyone.insert(names.begin(), names.end());
    }
    EXPECT_EQ(everyone.size(), static_cast<std::size_t>(kPlayers));  // nobody matched twice
}

// AuthServiceTest.cpp: register and login rules, using the in-memory store.
// Each call is async, so the helper below waits for the callback before checking.
#include <gtest/gtest.h>

#include <boost/asio/thread_pool.hpp>
#include <future>
#include <string>
#include <utility>

#include "auth/AuthService.h"
#include "store/InMemoryAccountStore.h"

namespace {

using matchforge::auth::AuthCallback;
using matchforge::auth::AuthResult;

// Errors the tests expect; they match the codes in AuthService.cpp.
constexpr std::int32_t kBadRequest = 400;
constexpr std::int32_t kInvalidCredentials = 401;
constexpr std::int32_t kUsernameTaken = 409;

class AuthServiceTest : public ::testing::Test {
protected:
    // Declared in dependency order: the pool is destroyed (and joined) before the store.
    matchforge::store::InMemoryAccountStore m_store;
    boost::asio::thread_pool m_workers{2};
    matchforge::auth::AuthService m_auth{m_store, m_workers};

    // Starts an operation and blocks until its callback has run.
    template <class Start>
    AuthResult wait(Start&& start) {
        std::promise<AuthResult> done;
        std::future<AuthResult> outcome = done.get_future();
        start([&done](AuthResult result) { done.set_value(std::move(result)); });
        return outcome.get();
    }

    AuthResult registerAccount(const std::string& username, const std::string& password) {
        return wait(
            [&](AuthCallback cb) { m_auth.registerAccount(username, password, std::move(cb)); });
    }

    AuthResult login(const std::string& username, const std::string& password) {
        return wait([&](AuthCallback cb) { m_auth.login(username, password, std::move(cb)); });
    }
};

}  // namespace

TEST_F(AuthServiceTest, RegisterCreatesAccountAndReturnsASession) {
    const AuthResult result = registerAccount("alice", "correct horse");

    ASSERT_TRUE(result.ok);
    EXPECT_EQ(result.username, "alice");
    EXPECT_EQ(result.sessionToken.size(), 43u);
    // The session really is stored, so a later request can recognise it.
    EXPECT_EQ(m_store.findSessionUser(result.sessionToken), "alice");
}

TEST_F(AuthServiceTest, RegisterRejectsAnInvalidUsername) {
    const AuthResult result = registerAccount("a b", "correct horse");

    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.errorCode, kBadRequest);
}

TEST_F(AuthServiceTest, RegisterRejectsAnInvalidPassword) {
    const AuthResult result = registerAccount("alice", "short");

    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.errorCode, kBadRequest);
}

TEST_F(AuthServiceTest, RegisterRejectsAUsernameThatIsAlreadyTaken) {
    ASSERT_TRUE(registerAccount("alice", "correct horse").ok);

    const AuthResult second = registerAccount("alice", "another password");

    EXPECT_FALSE(second.ok);
    EXPECT_EQ(second.errorCode, kUsernameTaken);
}

TEST_F(AuthServiceTest, UsernamesAreCaseInsensitive) {
    ASSERT_TRUE(registerAccount("Alice", "correct horse").ok);

    // "ALICE" is the same account, so it is taken, and login works with any capitalisation.
    EXPECT_EQ(registerAccount("ALICE", "another password").errorCode, kUsernameTaken);
    const AuthResult result = login("aLiCe", "correct horse");
    ASSERT_TRUE(result.ok);
    EXPECT_EQ(result.username, "alice");
}

TEST_F(AuthServiceTest, LoginSucceedsWithTheRightPassword) {
    ASSERT_TRUE(registerAccount("bob", "correct horse").ok);

    const AuthResult result = login("bob", "correct horse");

    ASSERT_TRUE(result.ok);
    EXPECT_EQ(m_store.findSessionUser(result.sessionToken), "bob");
}

TEST_F(AuthServiceTest, LoginFailsWithTheWrongPassword) {
    ASSERT_TRUE(registerAccount("bob", "correct horse").ok);

    const AuthResult result = login("bob", "wrong horse");

    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.errorCode, kInvalidCredentials);
    EXPECT_TRUE(result.sessionToken.empty());
}

TEST_F(AuthServiceTest, LoginForAnUnknownUserLooksTheSameAsAWrongPassword) {
    const AuthResult result = login("nobody", "correct horse");

    // Same code and message as a wrong password, so the reply does not reveal which usernames
    // exist.
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.errorCode, kInvalidCredentials);
    EXPECT_EQ(result.errorMessage, "invalid username or password");
}

TEST_F(AuthServiceTest, ErrorMessagesNeverEchoThePassword) {
    const std::string secret = "my-very-secret-password";
    ASSERT_TRUE(registerAccount("carol", secret).ok);

    const AuthResult wrong = login("carol", "not-the-password-1");
    const AuthResult invalid = registerAccount("x", secret);

    EXPECT_EQ(wrong.errorMessage.find(secret), std::string::npos);
    EXPECT_EQ(invalid.errorMessage.find(secret), std::string::npos);
}

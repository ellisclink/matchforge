// ServerTest.cpp: integration tests. A real Server listens on loopback, and a
// blocking client socket talks to it, so the whole path is exercised: accept,
// framing, protobuf, auth, reply.
#include <gtest/gtest.h>

#include <boost/asio/buffer.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>

#include "auth/AuthService.h"
#include "matchforge.pb.h"
#include "net/Framing.h"
#include "net/Server.h"
#include "store/InMemoryAccountStore.h"

namespace {

constexpr std::size_t kMaxFrameBytes = 1024;

// Runs an io_context on a background thread for the life of the object. The
// destructor stops it and joins, so a failed assertion cannot leave a thread running.
class IoThread {
public:
    // The work guard keeps run() from returning early. Without it, run() returns at once
    // when nothing is queued yet, and work posted later (such as accepting a connection)
    // would never run.
    explicit IoThread(boost::asio::io_context& ioContext)
        : m_ioContext(ioContext),
          m_guard(boost::asio::make_work_guard(ioContext)),
          m_thread([this] { m_ioContext.run(); }) {}

    ~IoThread() {
        m_guard.reset();
        m_ioContext.stop();
        m_thread.join();
    }

    IoThread(const IoThread&) = delete;
    IoThread& operator=(const IoThread&) = delete;

private:
    boost::asio::io_context& m_ioContext;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> m_guard;
    std::thread m_thread;
};

// A running server with accounts in memory. Members are declared in dependency order:
// they are destroyed in reverse, so the I/O thread stops before the server and the store go away.
class ServerFixture : public ::testing::Test {
protected:
    void SetUp() override { m_server.start(); }

    // The port the server picked (it binds to port 0, so any free port works).
    std::uint16_t port() const { return m_server.localPort(); }

    matchforge::store::InMemoryAccountStore m_store;
    boost::asio::thread_pool m_authWorkers{2};
    matchforge::auth::AuthService m_auth{m_store, m_authWorkers};
    boost::asio::io_context m_ioContext;
    matchforge::net::Server m_server{m_ioContext, /*port=*/0, kMaxFrameBytes, m_auth};
    IoThread m_runner{m_ioContext};  // Declared last, so it is destroyed first.
};

// Connects a blocking client socket to the loopback address on the given port.
boost::asio::ip::tcp::socket connectTo(boost::asio::io_context& ioContext, std::uint16_t port) {
    boost::asio::ip::tcp::socket socket(ioContext);
    socket.connect(
        boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), port));
    return socket;
}

// Sends one Envelope as a frame, the way a real client would.
void sendEnvelope(boost::asio::ip::tcp::socket& socket, const matchforge::v1::Envelope& envelope) {
    std::string bytes;
    ASSERT_TRUE(envelope.SerializeToString(&bytes));
    const std::string frame = matchforge::net::encodeFrame(bytes);
    boost::asio::write(socket, boost::asio::buffer(frame));
}

// Reads one complete frame from the socket and parses its payload as an Envelope.
// Returns false if the connection closed first.
bool receiveEnvelope(boost::asio::ip::tcp::socket& socket, matchforge::v1::Envelope& out) {
    std::string header(matchforge::net::kFrameHeaderBytes, '\0');
    boost::system::error_code ec;
    boost::asio::read(socket, boost::asio::buffer(header), ec);
    if (ec) {
        return false;
    }

    const auto* bytes = reinterpret_cast<const unsigned char*>(header.data());
    const std::uint32_t length = (static_cast<std::uint32_t>(bytes[0]) << 24) |
                                 (static_cast<std::uint32_t>(bytes[1]) << 16) |
                                 (static_cast<std::uint32_t>(bytes[2]) << 8) |
                                 static_cast<std::uint32_t>(bytes[3]);

    std::string body(length, '\0');
    boost::asio::read(socket, boost::asio::buffer(body), ec);
    if (ec) {
        return false;
    }
    return out.ParseFromString(body);
}

matchforge::v1::Envelope registerRequest(const std::string& username, const std::string& password) {
    matchforge::v1::Envelope envelope;
    envelope.mutable_register_()->set_username(username);
    envelope.mutable_register_()->set_password(password);
    return envelope;
}

matchforge::v1::Envelope loginRequest(const std::string& username, const std::string& password) {
    matchforge::v1::Envelope envelope;
    envelope.mutable_login()->set_username(username);
    envelope.mutable_login()->set_password(password);
    return envelope;
}

}  // namespace

// A heartbeat from the client should come back as a heartbeat from the server, with a clock value.
TEST_F(ServerFixture, RepliesToHeartbeatWithAHeartbeat) {
    auto socket = connectTo(m_ioContext, port());

    matchforge::v1::Envelope heartbeat;
    heartbeat.mutable_heartbeat()->set_sent_at_unix_ms(42);
    sendEnvelope(socket, heartbeat);

    matchforge::v1::Envelope reply;
    ASSERT_TRUE(receiveEnvelope(socket, reply));
    ASSERT_EQ(reply.payload_case(), matchforge::v1::Envelope::kHeartbeat);
    EXPECT_GT(reply.heartbeat().sent_at_unix_ms(), 0u);  // The server's clock is set.
}

// Garbage bytes that are not a valid Envelope get an ErrorResponse, then the connection closes.
TEST_F(ServerFixture, MalformedMessageGetsAnErrorThenDisconnect) {
    auto socket = connectTo(m_ioContext, port());

    // 0xFF is not a valid protobuf tag, so this payload fails to parse.
    const std::string garbage = matchforge::net::encodeFrame("\xff\xff\xff");
    boost::asio::write(socket, boost::asio::buffer(garbage));

    matchforge::v1::Envelope reply;
    ASSERT_TRUE(receiveEnvelope(socket, reply));
    ASSERT_EQ(reply.payload_case(), matchforge::v1::Envelope::kError);
    EXPECT_EQ(reply.error().code(), 400);

    // After the error, the server closes the connection, so the next read reports end of file.
    matchforge::v1::Envelope unused;
    EXPECT_FALSE(receiveEnvelope(socket, unused));
}

// A frame announcing more than the limit makes the server drop the connection without a reply.
TEST_F(ServerFixture, OversizedFrameDisconnectsWithoutReply) {
    auto socket = connectTo(m_ioContext, port());

    // Send only a header claiming 100000 bytes, far above the 1024-byte limit.
    const std::string header("\x00\x01\x86\xa0", 4);
    boost::asio::write(socket, boost::asio::buffer(header));

    matchforge::v1::Envelope unused;
    EXPECT_FALSE(receiveEnvelope(socket, unused));
}

// Registering over the connection returns a session token the client can keep.
TEST_F(ServerFixture, RegisterReturnsASessionToken) {
    auto socket = connectTo(m_ioContext, port());

    sendEnvelope(socket, registerRequest("dave", "correct horse"));

    matchforge::v1::Envelope reply;
    ASSERT_TRUE(receiveEnvelope(socket, reply));
    ASSERT_EQ(reply.payload_case(), matchforge::v1::Envelope::kAuth);
    EXPECT_EQ(reply.auth().username(), "dave");
    EXPECT_EQ(reply.auth().session_token().size(), 43u);
}

// The same name cannot be registered twice. The second attempt gets a 409 error on the same
// connection.
TEST_F(ServerFixture, DuplicateRegisterGetsAConflictError) {
    auto socket = connectTo(m_ioContext, port());

    sendEnvelope(socket, registerRequest("erin", "correct horse"));
    matchforge::v1::Envelope first;
    ASSERT_TRUE(receiveEnvelope(socket, first));
    ASSERT_EQ(first.payload_case(), matchforge::v1::Envelope::kAuth);

    sendEnvelope(socket, registerRequest("erin", "another password"));
    matchforge::v1::Envelope second;
    ASSERT_TRUE(receiveEnvelope(socket, second));
    ASSERT_EQ(second.payload_case(), matchforge::v1::Envelope::kError);
    EXPECT_EQ(second.error().code(), 409);
}

// A client can log in to an account it registered, and the connection stays open afterwards.
TEST_F(ServerFixture, LoginAfterRegisterReturnsASessionAndKeepsTheConnection) {
    auto socket = connectTo(m_ioContext, port());

    sendEnvelope(socket, registerRequest("frank", "correct horse"));
    matchforge::v1::Envelope registered;
    ASSERT_TRUE(receiveEnvelope(socket, registered));
    ASSERT_EQ(registered.payload_case(), matchforge::v1::Envelope::kAuth);

    sendEnvelope(socket, loginRequest("frank", "correct horse"));
    matchforge::v1::Envelope loggedIn;
    ASSERT_TRUE(receiveEnvelope(socket, loggedIn));
    ASSERT_EQ(loggedIn.payload_case(), matchforge::v1::Envelope::kAuth);

    // Replies come back in the order requests were sent, and the connection is still usable.
    matchforge::v1::Envelope heartbeat;
    heartbeat.mutable_heartbeat()->set_sent_at_unix_ms(1);
    sendEnvelope(socket, heartbeat);
    matchforge::v1::Envelope heartbeatReply;
    ASSERT_TRUE(receiveEnvelope(socket, heartbeatReply));
    EXPECT_EQ(heartbeatReply.payload_case(), matchforge::v1::Envelope::kHeartbeat);
}

// A wrong password gets a 401 error, and the message does not reveal whether the account exists.
TEST_F(ServerFixture, WrongPasswordGetsAnAuthError) {
    auto socket = connectTo(m_ioContext, port());

    sendEnvelope(socket, registerRequest("gina", "correct horse"));
    matchforge::v1::Envelope registered;
    ASSERT_TRUE(receiveEnvelope(socket, registered));

    sendEnvelope(socket, loginRequest("gina", "wrong horse"));
    matchforge::v1::Envelope reply;
    ASSERT_TRUE(receiveEnvelope(socket, reply));
    ASSERT_EQ(reply.payload_case(), matchforge::v1::Envelope::kError);
    EXPECT_EQ(reply.error().code(), 401);
    EXPECT_EQ(reply.error().message(), "invalid username or password");
}

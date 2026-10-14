// TestSupport.h: shared helpers for tests that talk to a real server over loopback.
//
//   ServerFixture  a complete server (in-memory accounts, lobby, listener) on a free port
//   TestClient     a blocking client socket that sends and receives framed Envelopes
//
// Every receive has a timeout, so a missing reply fails the test instead of hanging it.
#pragma once

#include <gtest/gtest.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <boost/asio/buffer.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>

#include "auth/AuthService.h"
#include "lobby/LobbyService.h"
#include "matchforge.pb.h"
#include "metrics/Metrics.h"
#include "net/Framing.h"
#include "net/Server.h"
#include "store/InMemoryAccountStore.h"

namespace matchforge::test {

// Runs an io_context on a background thread for the life of the object. The work guard
// keeps run() from returning early when nothing is queued yet. The destructor stops it and
// joins, so a failed assertion cannot leave a thread running.
class IoThread {
public:
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

inline constexpr const char* kResultSecret = "test-result-secret";

// Limits used by most tests: small frames, generous rates and timeouts so slow sanitizer
// builds never trip them by accident.
inline net::SessionLimits defaultTestLimits() {
    net::SessionLimits limits;
    limits.maxFrameBytes = 1024;
    limits.idleTimeout = std::chrono::seconds(60);
    limits.messagesPerSecond = 1000;
    limits.messageBurst = 1000;
    return limits;
}

inline lobby::LobbySettings testLobbySettings() {
    lobby::LobbySettings settings;
    settings.modes = {{"duel", 1}, {"duo", 2}};
    settings.regions = {"eu", "na"};
    settings.tickInterval = std::chrono::milliseconds(20);  // fast matching keeps tests quick
    settings.resultSecret = kResultSecret;
    return settings;
}

// A complete running server (accounts in memory, lobby, listener on a free port) with the
// given limits. Members are declared in dependency order: they are destroyed in reverse,
// so the I/O thread stops before anything it uses goes away.
class TestServer {
public:
    explicit TestServer(net::SessionLimits limits = defaultTestLimits(),
                        core::LoginThrottleSettings throttle = {})
        : m_auth(m_store, m_workers, throttle, &m_metrics),
          m_lobby(m_ioContext, m_store, m_workers, testLobbySettings(), &m_metrics),
          m_server(m_ioContext, /*port=*/0, limits, m_auth, m_lobby, &m_metrics),
          m_runner(m_ioContext) {
        m_lobby.start();
        m_server.start();
    }

    ~TestServer() { m_lobby.stop(); }

    TestServer(const TestServer&) = delete;
    TestServer& operator=(const TestServer&) = delete;

    // The port the server picked (it binds to port 0, so any free port works).
    std::uint16_t port() const { return m_server.localPort(); }
    store::InMemoryAccountStore& store() { return m_store; }
    metrics::Metrics& metrics() { return m_metrics; }

private:
    metrics::Metrics m_metrics;  // First, so it outlives everything that records into it.
    store::InMemoryAccountStore m_store;
    boost::asio::thread_pool m_workers{2};
    auth::AuthService m_auth;
    boost::asio::io_context m_ioContext;
    lobby::LobbyService m_lobby;
    net::Server m_server;
    IoThread m_runner;  // Declared last, so it is destroyed first.
};

// GoogleTest fixture: a fresh TestServer with default limits for every test.
class ServerFixture : public ::testing::Test {
protected:
    static constexpr const char* kResultSecret = test::kResultSecret;

    std::uint16_t port() const { return m_testServer.port(); }

    TestServer m_testServer;
    store::InMemoryAccountStore& m_store = m_testServer.store();
};

// A blocking client, like a very small game client. It uses its own io_context, so many
// clients can run on separate test threads at once.
class TestClient {
public:
    explicit TestClient(std::uint16_t port) : m_socket(m_ioContext) {
        m_socket.connect(
            boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), port));

        // A 10-second receive timeout at the OS level: a read with no data fails instead of
        // blocking forever.
        timeval timeout{};
        timeout.tv_sec = 10;
        setsockopt(m_socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    }

    // Sends one Envelope as a frame.
    void send(const v1::Envelope& envelope) {
        std::string bytes;
        envelope.SerializeToString(&bytes);
        sendRaw(net::encodeFrame(bytes));
    }

    // Sends raw bytes, for tests that need to send something malformed.
    void sendRaw(const std::string& bytes) {
        boost::asio::write(m_socket, boost::asio::buffer(bytes));
    }

    // Reads one framed Envelope. Returns false if the connection closed, the read timed out,
    // or the payload did not parse.
    bool receive(v1::Envelope& out) {
        std::string header(net::kFrameHeaderBytes, '\0');
        boost::system::error_code ec;
        boost::asio::read(m_socket, boost::asio::buffer(header), ec);
        if (ec) {
            return false;
        }
        const auto* bytes = reinterpret_cast<const unsigned char*>(header.data());
        const std::uint32_t length = (static_cast<std::uint32_t>(bytes[0]) << 24) |
                                     (static_cast<std::uint32_t>(bytes[1]) << 16) |
                                     (static_cast<std::uint32_t>(bytes[2]) << 8) |
                                     static_cast<std::uint32_t>(bytes[3]);
        std::string body(length, '\0');
        boost::asio::read(m_socket, boost::asio::buffer(body), ec);
        if (ec) {
            return false;
        }
        return out.ParseFromString(body);
    }

    // Receives one Envelope and checks its type. Records a test failure if it is not `expected`.
    v1::Envelope expect(v1::Envelope::PayloadCase expected) {
        v1::Envelope envelope;
        if (!receive(envelope)) {
            ADD_FAILURE() << "connection closed or timed out waiting for payload case " << expected;
            return envelope;
        }
        EXPECT_EQ(envelope.payload_case(), expected) << "got: " << envelope.ShortDebugString();
        return envelope;
    }

    // Registers a new account and returns the server's AuthResponse.
    v1::AuthResponse registerAs(const std::string& username,
                                const std::string& password = "correct horse") {
        v1::Envelope request;
        request.mutable_register_()->set_username(username);
        request.mutable_register_()->set_password(password);
        send(request);
        return expect(v1::Envelope::kAuth).auth();
    }

    v1::AuthResponse loginAs(const std::string& username,
                             const std::string& password = "correct horse") {
        v1::Envelope request;
        request.mutable_login()->set_username(username);
        request.mutable_login()->set_password(password);
        send(request);
        return expect(v1::Envelope::kAuth).auth();
    }

    // Asks to join a queue. Does not wait for the answer.
    void joinQueue(const std::string& mode, const std::string& region) {
        v1::Envelope request;
        request.mutable_join_queue()->set_mode(mode);
        request.mutable_join_queue()->set_region(region);
        send(request);
    }

    void leaveQueue() {
        v1::Envelope request;
        request.mutable_leave_queue();
        send(request);
    }

    void reportResult(const std::string& matchId, int winningTeam, const std::string& secret) {
        v1::Envelope request;
        auto* report = request.mutable_report_result();
        report->set_match_id(matchId);
        report->set_winning_team(winningTeam);
        report->set_result_secret(secret);
        send(request);
    }

    void close() {
        boost::system::error_code ignored;
        m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
        m_socket.close(ignored);
    }

private:
    boost::asio::io_context m_ioContext;
    boost::asio::ip::tcp::socket m_socket;
};

}  // namespace matchforge::test

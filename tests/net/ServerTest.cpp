// ServerTest.cpp: integration tests. A real Server listens on loopback, and a
// blocking client socket talks to it, so the whole path is exercised: accept,
// framing, protobuf, reply.
#include <gtest/gtest.h>

#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>

#include "matchforge.pb.h"
#include "net/Framing.h"
#include "net/Server.h"

namespace {

constexpr std::size_t kMaxFrameBytes = 1024;

// Runs an io_context on a background thread for the life of the object. The
// destructor stops it and joins, so a failed assertion cannot leave a thread running.
class IoThread {
public:
    explicit IoThread(boost::asio::io_context& ioContext)
        : m_ioContext(ioContext), m_thread([this] { m_ioContext.run(); }) {}

    ~IoThread() {
        m_ioContext.stop();
        m_thread.join();
    }

    IoThread(const IoThread&) = delete;
    IoThread& operator=(const IoThread&) = delete;

private:
    boost::asio::io_context& m_ioContext;
    std::thread m_thread;
};

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

}  // namespace

// A heartbeat from the client should come back as a heartbeat from the server, with a clock value.
TEST(ServerTest, RepliesToHeartbeatWithAHeartbeat) {
    boost::asio::io_context ioContext;
    matchforge::net::Server server(ioContext, /*port=*/0, kMaxFrameBytes);
    server.start();
    IoThread runner(ioContext);

    boost::asio::ip::tcp::socket socket(ioContext);
    socket.connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"),
                                                  server.localPort()));

    matchforge::v1::Envelope heartbeat;
    heartbeat.mutable_heartbeat()->set_sent_at_unix_ms(42);
    sendEnvelope(socket, heartbeat);

    matchforge::v1::Envelope reply;
    ASSERT_TRUE(receiveEnvelope(socket, reply));
    ASSERT_EQ(reply.payload_case(), matchforge::v1::Envelope::kHeartbeat);
    EXPECT_GT(reply.heartbeat().sent_at_unix_ms(), 0u);  // The server's clock is set.
}

// Garbage bytes that are not a valid Envelope get an ErrorResponse, then the connection closes.
TEST(ServerTest, MalformedMessageGetsAnErrorThenDisconnect) {
    boost::asio::io_context ioContext;
    matchforge::net::Server server(ioContext, /*port=*/0, kMaxFrameBytes);
    server.start();
    IoThread runner(ioContext);

    boost::asio::ip::tcp::socket socket(ioContext);
    socket.connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"),
                                                  server.localPort()));

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
TEST(ServerTest, OversizedFrameDisconnectsWithoutReply) {
    boost::asio::io_context ioContext;
    matchforge::net::Server server(ioContext, /*port=*/0, kMaxFrameBytes);
    server.start();
    IoThread runner(ioContext);

    boost::asio::ip::tcp::socket socket(ioContext);
    socket.connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"),
                                                  server.localPort()));

    // Send only a header claiming 100000 bytes, far above the 1024-byte limit.
    const std::string header("\x00\x01\x86\xa0", 4);
    boost::asio::write(socket, boost::asio::buffer(header));

    matchforge::v1::Envelope unused;
    EXPECT_FALSE(receiveEnvelope(socket, unused));
}

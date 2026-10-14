// BotClient.h: a small blocking client for the matchforge protocol.
//
// Each bot runs on its own thread, so blocking calls are fine here (the rule about never
// blocking applies to the server's I/O threads, not to a load-test client).
#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdint>
#include <string>

#include "matchforge.pb.h"

namespace matchforge::bots {

class BotClient {
public:
    // Connects to host:port. Every later read fails if no data arrives within
    // timeoutSeconds. Throws boost::system::system_error if the connection fails.
    BotClient(const std::string& host, std::uint16_t port, int timeoutSeconds);

    // Sends one Envelope as a frame. Throws on a broken connection.
    void send(const v1::Envelope& envelope);

    // Reads one Envelope. Returns false if the connection closed, a read timed out, or the
    // bytes did not parse.
    bool receive(v1::Envelope& out);

    // Reads until a message of type `wanted` arrives, skipping other messages. Returns false
    // if an ErrorResponse arrives first (left in `out`), or if receive() fails.
    bool waitFor(v1::Envelope::PayloadCase wanted, v1::Envelope& out);

private:
    boost::asio::io_context m_ioContext;
    boost::asio::ip::tcp::socket m_socket;
};

}  // namespace matchforge::bots

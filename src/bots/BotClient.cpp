// BotClient.cpp: connect, send and receive framed Envelopes with blocking calls.
#include "bots/BotClient.h"

#include <sys/socket.h>
#include <sys/time.h>

#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>

#include "net/Framing.h"

namespace matchforge::bots {

BotClient::BotClient(const std::string& host, std::uint16_t port, int timeoutSeconds)
    : m_socket(m_ioContext) {
    // The resolver turns a name like "server" (a Docker service) or "127.0.0.1" into addresses.
    boost::asio::ip::tcp::resolver resolver(m_ioContext);
    boost::asio::connect(m_socket, resolver.resolve(host, std::to_string(port)));

    // An OS-level receive timeout: a read with no data fails instead of blocking forever,
    // so a stuck server shows up as an error in the report instead of a hung bot.
    timeval timeout{};
    timeout.tv_sec = timeoutSeconds;
    setsockopt(m_socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}

void BotClient::send(const v1::Envelope& envelope) {
    std::string bytes;
    envelope.SerializeToString(&bytes);
    const std::string frame = net::encodeFrame(bytes);
    boost::asio::write(m_socket, boost::asio::buffer(frame));
}

bool BotClient::receive(v1::Envelope& out) {
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

bool BotClient::waitFor(v1::Envelope::PayloadCase wanted, v1::Envelope& out) {
    while (receive(out)) {
        if (out.payload_case() == wanted) {
            return true;
        }
        if (out.payload_case() == v1::Envelope::kError) {
            return false;
        }
        // Anything else (for example a QueueStatus while waiting for MatchFound) is skipped.
    }
    return false;
}

}  // namespace matchforge::bots

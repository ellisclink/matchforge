// Server.h: the listening socket. It accepts connections and starts a Session for each one.
#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstddef>
#include <cstdint>

namespace matchforge::net {

class Server {
public:
    // Opens and binds the listening socket right away, so a port that is already in use
    // fails here with an exception rather than later. Port 0 asks the OS for any free port;
    // localPort() then reports which one it picked (tests use this).
    Server(boost::asio::io_context& ioContext, std::uint16_t port, std::size_t maxFrameBytes);

    // Starts accepting connections. Connections are handled on the io_context's threads.
    void start();

    // Stops accepting new connections. Sessions already open finish on their own.
    void stop();

    // The port the server is actually listening on.
    std::uint16_t localPort() const;

private:
    void doAccept();  // Waits for one client, then starts a session and waits again.

    boost::asio::ip::tcp::acceptor m_acceptor;
    std::size_t m_maxFrameBytes;
};

}  // namespace matchforge::net

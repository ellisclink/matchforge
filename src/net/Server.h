// Server.h: the listening socket. It accepts connections and starts a Session for each one.
#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstddef>
#include <cstdint>

#include "auth/AuthService.h"
#include "lobby/LobbyService.h"
#include "net/Session.h"

namespace matchforge::metrics {
class Metrics;  // Defined in metrics/Metrics.h; only a pointer is used here.
}

namespace matchforge::net {

class Server {
public:
    // Opens and binds the listening socket right away, so a port that is already in use
    // fails here with an exception rather than later. Port 0 asks the OS for any free port;
    // localPort() then reports which one it picked (tests use this).
    // The auth and lobby services must outlive the server.
    Server(boost::asio::io_context& ioContext, std::uint16_t port, const SessionLimits& limits,
           auth::AuthService& auth, lobby::LobbyService& lobby,
           metrics::Metrics* metrics = nullptr);

    // Starts accepting connections. Connections are handled on the io_context's threads.
    void start();

    // Stops accepting new connections. Sessions already open finish on their own.
    void stop();

    // The port the server is actually listening on.
    std::uint16_t localPort() const;

private:
    void doAccept();  // Waits for one client, then starts a session and waits again.

    boost::asio::ip::tcp::acceptor m_acceptor;
    SessionLimits m_limits;  // Applied to every connection.
    auth::AuthService& m_auth;
    lobby::LobbyService& m_lobby;
    metrics::Metrics* m_metrics;  // May be null.
};

}  // namespace matchforge::net

// Server.cpp: the accept loop.
#include "net/Server.h"

#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>
#include <memory>
#include <utility>

#include "net/Session.h"

namespace matchforge::net {

namespace {

// Listen on every network interface, so the server is reachable from other machines
// and from inside Docker. This is safe for a skeleton. Restrict it later if needed.
boost::asio::ip::tcp::endpoint listenEndpoint(std::uint16_t port) {
    return boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port);
}

}  // namespace

Server::Server(boost::asio::io_context& ioContext, std::uint16_t port, const SessionLimits& limits,
               auth::AuthService& auth, lobby::LobbyService& lobby)
    : m_acceptor(ioContext,
                 listenEndpoint(port)),  // Binds and listens now; throws if the port is taken.
      m_limits(limits),
      m_auth(auth),
      m_lobby(lobby) {}

void Server::start() { doAccept(); }

void Server::stop() {
    // Closing the acceptor makes any pending accept fail, which ends the accept loop.
    boost::system::error_code ignored;
    m_acceptor.close(ignored);
}

std::uint16_t Server::localPort() const {
    boost::system::error_code ignored;
    return m_acceptor.local_endpoint(ignored).port();
}

void Server::doAccept() {
    // The handler captures `this`, so the Server must outlive the io_context run.
    // main() and the tests both keep it alive until the io_context has stopped.
    m_acceptor.async_accept(
        [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (!ec) {
                // Each client gets its own Session, owned by shared_ptr until its work finishes.
                std::make_shared<Session>(std::move(socket), m_limits, m_auth, m_lobby)->start();
            }

            // Keep accepting, unless stop() closed the acceptor. A failed accept (for example,
            // a temporary resource error) should not end the server, so we keep going.
            if (m_acceptor.is_open()) {
                doAccept();
            }
        });
}

}  // namespace matchforge::net

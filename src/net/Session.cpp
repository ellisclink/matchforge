// Session.cpp: the per-connection logic: read, decode, handle, reply.
#include "net/Session.h"

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <string_view>
#include <utility>

#include "matchforge.pb.h"
#include "net/Framing.h"

namespace matchforge::net {

namespace {

// Error codes sent to the client in ErrorResponse. They loosely follow HTTP.
constexpr std::int32_t kBadRequest = 400;    // the request itself was wrong
constexpr std::int32_t kUnauthorized = 401;  // the client must log in first

// The current time as milliseconds since the Unix epoch, for Heartbeat replies.
std::uint64_t unixMillisNow() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

}  // namespace

Session::Session(boost::asio::ip::tcp::socket socket, std::size_t maxFrameBytes,
                 auth::AuthService& auth, lobby::LobbyService& lobby)
    : m_socket(std::move(socket)),
      m_auth(auth),
      m_lobby(lobby),
      m_strand(boost::asio::make_strand(m_socket.get_executor())),
      m_decoder(maxFrameBytes) {}

void Session::start() {
    // Post to the strand so the first read also runs inside the session's
    // serialized context, the same place as every later handler.
    boost::asio::post(m_strand, [self = shared_from_this()] { self->doRead(); });
}

void Session::deliver(std::string serializedEnvelope) {
    // Called from the lobby's strand. Hop onto ours before touching the write queue.
    boost::asio::post(m_strand, [self = shared_from_this(), bytes = std::move(serializedEnvelope)] {
        if (!self->m_closed) {
            self->sendEnvelope(bytes);
        }
    });
}

void Session::doRead() {
    m_socket.async_read_some(
        boost::asio::buffer(m_readBuffer),
        boost::asio::bind_executor(
            m_strand,
            [self = shared_from_this()](boost::system::error_code ec, std::size_t bytesRead) {
                // An error here usually means the client disconnected. Nothing else to do.
                if (ec) {
                    self->close();
                    return;
                }

                self->m_decoder.append(std::string_view(self->m_readBuffer.data(), bytesRead));

                // One read can contain several whole frames, so keep pulling until none are left.
                while (auto payload = self->m_decoder.nextFrame()) {
                    self->handleFrame(*payload);
                    if (self->m_closed || self->m_closeAfterWrite) {
                        return;
                    }
                }

                // The client announced a frame bigger than our limit. Drop the connection
                // instead of buffering data we would reject anyway.
                if (self->m_decoder.failed()) {
                    self->close();
                    return;
                }

                self->doRead();  // Wait for more bytes.
            }));
}

void Session::handleFrame(const std::string& payload) {
    matchforge::v1::Envelope request;
    if (!request.ParseFromString(payload)) {
        // The bytes are not a valid Envelope. Tell the client, then close, since the stream
        // can no longer be trusted to line up on frame boundaries.
        sendError(kBadRequest, "malformed message");
        closeAfterFlush();
        return;
    }

    // Auth and lobby work happens on other threads. Their replies come back through
    // callbacks or deliver(), which hop onto this session's strand before touching state.
    switch (request.payload_case()) {
        case matchforge::v1::Envelope::kHeartbeat: {
            // Echo a heartbeat with the server's clock, so the client can see the server is alive.
            matchforge::v1::Envelope reply;
            reply.mutable_heartbeat()->set_sent_at_unix_ms(unixMillisNow());
            std::string bytes;
            reply.SerializeToString(&bytes);
            sendEnvelope(bytes);
            break;
        }
        case matchforge::v1::Envelope::kRegister: {
            const auto& registerRequest = request.register_();
            m_auth.registerAccount(registerRequest.username(), registerRequest.password(),
                                   [self = shared_from_this()](auth::AuthResult result) {
                                       boost::asio::post(self->m_strand,
                                                         [self, result = std::move(result)] {
                                                             self->handleAuthResult(result);
                                                         });
                                   });
            break;
        }
        case matchforge::v1::Envelope::kLogin: {
            const auto& loginRequest = request.login();
            m_auth.login(loginRequest.username(), loginRequest.password(),
                         [self = shared_from_this()](auth::AuthResult result) {
                             boost::asio::post(self->m_strand, [self, result = std::move(result)] {
                                 self->handleAuthResult(result);
                             });
                         });
            break;
        }
        case matchforge::v1::Envelope::kJoinQueue: {
            // Only a logged-in connection may queue: the lobby needs a real account name.
            if (!m_username) {
                sendError(kUnauthorized, "log in before joining a queue");
                break;
            }
            const auto& join = request.join_queue();
            m_lobby.join(*m_username, join.mode(), join.region(), weak_from_this());
            break;
        }
        case matchforge::v1::Envelope::kLeaveQueue: {
            if (!m_username) {
                sendError(kUnauthorized, "log in before leaving a queue");
                break;
            }
            m_lobby.leave(*m_username, weak_from_this());
            break;
        }
        case matchforge::v1::Envelope::kReportResult: {
            // Game servers report results; they prove themselves with the shared secret, not
            // with a player login. The lobby checks the secret.
            const auto& report = request.report_result();
            m_lobby.reportResult(report.match_id(), report.winning_team(), report.result_secret(),
                                 weak_from_this());
            break;
        }
        case matchforge::v1::Envelope::PAYLOAD_NOT_SET:
        case matchforge::v1::Envelope::kError:
        case matchforge::v1::Envelope::kAuth:
        case matchforge::v1::Envelope::kQueueStatus:
        case matchforge::v1::Envelope::kMatchFound:
        case matchforge::v1::Envelope::kResultAck: {
            // These are server-to-client messages (or nothing at all). A client sending one is
            // a bad request, but not a broken stream, so the connection stays open.
            sendError(kBadRequest, "unsupported message");
            break;
        }
    }
}

void Session::handleAuthResult(const auth::AuthResult& result) {
    if (!result.ok) {
        sendError(result.errorCode, result.errorMessage);
        return;
    }

    // Logging in as someone else on the same connection: take the old name out of the queue.
    if (m_username && *m_username != result.username) {
        m_lobby.disconnect(*m_username, weak_from_this());
    }
    m_username = result.username;

    // The token is sent only to the connection that asked for it. It is never logged.
    matchforge::v1::Envelope reply;
    auto* authReply = reply.mutable_auth();
    authReply->set_session_token(result.sessionToken);
    authReply->set_username(result.username);
    authReply->set_rating(result.rating);
    std::string bytes;
    reply.SerializeToString(&bytes);
    sendEnvelope(bytes);
}

void Session::sendError(std::int32_t code, const std::string& message) {
    matchforge::v1::Envelope reply;
    auto* error = reply.mutable_error();
    error->set_code(code);
    error->set_message(message);
    std::string bytes;
    reply.SerializeToString(&bytes);
    sendEnvelope(bytes);
}

void Session::sendEnvelope(const std::string& serializedEnvelope) {
    enqueue(encodeFrame(serializedEnvelope));
}

void Session::enqueue(std::string frame) {
    m_writeQueue.push_back(std::move(frame));
    // Only one async_write may run at a time on a socket. If the queue was already
    // busy, the running write will pick this frame up when it finishes.
    if (m_writeQueue.size() == 1) {
        doWrite();
    }
}

void Session::doWrite() {
    boost::asio::async_write(
        m_socket, boost::asio::buffer(m_writeQueue.front()),
        boost::asio::bind_executor(
            m_strand, [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
                if (ec) {
                    self->close();
                    return;
                }

                self->m_writeQueue.pop_front();
                if (!self->m_writeQueue.empty()) {
                    self->doWrite();  // More replies are waiting.
                } else if (self->m_closeAfterWrite) {
                    self->close();  // The last reply has been sent, so the session is done.
                }
            }));
}

void Session::closeAfterFlush() {
    // Do not close right away: the queued error reply must reach the client first.
    m_closeAfterWrite = true;
    if (m_writeQueue.empty()) {
        close();
    }
}

void Session::close() {
    if (m_closed) {
        return;  // Closing twice is harmless, but skip the work.
    }
    m_closed = true;

    // A player who disconnects while queued must not be matched afterwards.
    if (m_username) {
        m_lobby.disconnect(*m_username, weak_from_this());
    }

    // The error_code overloads report failures in the error_code instead of throwing,
    // which is what we want while tearing a connection down.
    boost::system::error_code ignored;
    m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
    m_socket.close(ignored);
}

}  // namespace matchforge::net

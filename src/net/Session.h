// Session.h: one connected client. It reads frames, answers them, and writes replies.
#pragma once

#include <array>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <cstddef>
#include <deque>
#include <memory>
#include <string>

#include "net/Framing.h"

namespace matchforge::net {

// A Session lives as long as its connection. It is shared with the Asio handlers
// that are waiting on it, so it is created with std::make_shared and inherits
// enable_shared_from_this. That keeps it alive until the last pending operation finishes.
class Session : public std::enable_shared_from_this<Session> {
public:
    // Takes ownership of an accepted socket. maxFrameBytes is the largest message we accept from
    // this client.
    Session(boost::asio::ip::tcp::socket socket, std::size_t maxFrameBytes);

    // Starts reading from the client. Call exactly once, right after creating the session.
    void start();

private:
    // A strand runs its handlers one at a time, never in parallel. Each session has its
    // own strand, so its reads and writes never race, and no mutex is needed.
    using Strand = boost::asio::strand<boost::asio::any_io_executor>;

    void doRead();                                             // Waits for the next chunk of bytes.
    void handleFrame(const std::string& payload);              // Processes one complete message.
    void sendEnvelope(const std::string& serializedEnvelope);  // Frames and queues a reply.
    void enqueue(std::string frame);  // Adds a frame to the write queue and starts writing if idle.
    void doWrite();                   // Writes the frame at the front of the queue.
    void closeAfterFlush();           // Closes once queued replies have been sent.
    void close();                     // Closes the socket now.

    boost::asio::ip::tcp::socket m_socket;
    Strand m_strand;
    FrameDecoder m_decoder;
    std::array<char, 4096> m_readBuffer{};  // Scratch space for one read from the socket.
    std::deque<std::string> m_writeQueue;   // Framed replies waiting to be written, in order.
    bool m_closeAfterWrite = false;  // Set when the last queued reply should be the final message.
    bool m_closed = false;           // True after close(); stops further work.
};

}  // namespace matchforge::net

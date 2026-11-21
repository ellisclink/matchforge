// Framing.h: splits a TCP byte stream into messages.
//
// TCP has no message boundaries: one read can return half a message, or three
// messages glued together. A frame adds a 4-byte length in front of each
// message, so the receiver knows where each one ends.
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace matchforge::net {

// Bytes in the length prefix. 4 bytes allows lengths up to about 4 GB.
inline constexpr std::size_t kFrameHeaderBytes = 4;

// Returns payload with its length in front: 4 bytes, big-endian (most
// significant byte first), then the payload bytes. Big-endian is the usual
// network byte order, so any machine reads the length the same way.
std::string encodeFrame(std::string_view payload);

// Collects bytes as they arrive and hands back complete payloads one at a time.
class FrameDecoder {
public:
    // maxPayloadBytes is the largest payload we accept. A larger length means the
    // peer is broken or hostile, so the decoder enters a failed state instead of
    // waiting for gigabytes of data.
    explicit FrameDecoder(std::size_t maxPayloadBytes);

    // Adds newly received bytes to the internal buffer. Call this after every read.
    void append(std::string_view bytes);

    // Returns the next complete payload, or std::nullopt if more bytes are needed
    // or the decoder has failed. Call in a loop until it returns nullopt.
    std::optional<std::string> nextFrame();

    // True after a frame announced a payload larger than the limit. The caller should close the
    // connection.
    bool failed() const { return m_failed; }

private:
    std::string m_buffer;           // Bytes received but not yet returned as frames.
    std::size_t m_maxPayloadBytes;  // Largest payload allowed (the configured limit).
    bool m_failed = false;          // Set once an oversized frame is seen. Never cleared.
};

}  // namespace matchforge::net

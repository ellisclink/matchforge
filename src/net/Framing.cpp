// Framing.cpp: the encoder and decoder declared in Framing.h.
#include "net/Framing.h"

#include <cstdint>
#include <stdexcept>

namespace matchforge::net {

std::string encodeFrame(std::string_view payload) {
    // The length must fit in the 4-byte header. Anything larger cannot be described.
    if (payload.size() > UINT32_MAX) {
        throw std::length_error("payload too large for a 4-byte frame header");
    }

    const auto length = static_cast<std::uint32_t>(payload.size());

    std::string frame;
    frame.reserve(kFrameHeaderBytes + payload.size());

    // Write the length one byte at a time, most significant byte first (big-endian).
    // Doing it by hand avoids depending on the machine's own byte order.
    frame.push_back(static_cast<char>((length >> 24) & 0xFF));
    frame.push_back(static_cast<char>((length >> 16) & 0xFF));
    frame.push_back(static_cast<char>((length >> 8) & 0xFF));
    frame.push_back(static_cast<char>(length & 0xFF));

    frame.append(payload);
    return frame;
}

FrameDecoder::FrameDecoder(std::size_t maxPayloadBytes) : m_maxPayloadBytes(maxPayloadBytes) {}

void FrameDecoder::append(std::string_view bytes) {
    // Once failed, the stream cannot be trusted, so new bytes are ignored.
    if (m_failed) {
        return;
    }
    m_buffer.append(bytes);
}

std::optional<std::string> FrameDecoder::nextFrame() {
    if (m_failed || m_buffer.size() < kFrameHeaderBytes) {
        return std::nullopt;  // Not even a full header yet.
    }

    // Rebuild the 4-byte big-endian length. Cast to unsigned char first so the
    // high bit of each byte is not treated as a sign.
    const auto* bytes = reinterpret_cast<const unsigned char*>(m_buffer.data());
    const std::uint32_t length = (static_cast<std::uint32_t>(bytes[0]) << 24) |
                                 (static_cast<std::uint32_t>(bytes[1]) << 16) |
                                 (static_cast<std::uint32_t>(bytes[2]) << 8) |
                                 static_cast<std::uint32_t>(bytes[3]);

    // Reject the frame before reading its body, so a huge length cannot force
    // us to buffer memory we will never use.
    if (length > m_maxPayloadBytes) {
        m_failed = true;
        return std::nullopt;
    }

    const std::size_t frameSize = kFrameHeaderBytes + length;
    if (m_buffer.size() < frameSize) {
        return std::nullopt;  // Header is here, but the body has not fully arrived yet.
    }

    std::string payload = m_buffer.substr(kFrameHeaderBytes, length);
    m_buffer.erase(0, frameSize);  // Drop the frame we just returned.
    return payload;
}

}  // namespace matchforge::net

// FramingTest.cpp: unit tests for the frame encoder and decoder. No sockets are used here.
#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <string_view>

#include "net/Framing.h"

namespace {

constexpr std::size_t kLargeLimit = 1024;  // A limit high enough that these tests never hit it.

}  // namespace

// The length prefix must be big-endian, so 3 is 00 00 00 03 on any machine.
TEST(FramingTest, EncodeWritesBigEndianLengthHeader) {
    const std::string frame = matchforge::net::encodeFrame("abc");

    ASSERT_EQ(frame.size(), 7u);  // 4 header bytes + 3 payload bytes.
    EXPECT_EQ(static_cast<unsigned char>(frame[0]), 0x00);
    EXPECT_EQ(static_cast<unsigned char>(frame[1]), 0x00);
    EXPECT_EQ(static_cast<unsigned char>(frame[2]), 0x00);
    EXPECT_EQ(static_cast<unsigned char>(frame[3]), 0x03);
    EXPECT_EQ(frame.substr(4), "abc");
}

// A frame that arrives in one piece should decode back to the same payload.
TEST(FramingTest, DecodesOneCompleteFrame) {
    matchforge::net::FrameDecoder decoder(kLargeLimit);
    decoder.append(matchforge::net::encodeFrame("hello"));

    const auto payload = decoder.nextFrame();
    ASSERT_TRUE(payload.has_value());
    EXPECT_EQ(*payload, "hello");
    EXPECT_FALSE(decoder.nextFrame().has_value());  // Nothing left over.
}

// TCP can deliver a frame one byte at a time. The decoder must wait, then return it whole.
TEST(FramingTest, WaitsForTheWholeFrameWhenBytesArriveSlowly) {
    const std::string frame = matchforge::net::encodeFrame("split");
    matchforge::net::FrameDecoder decoder(kLargeLimit);

    for (std::size_t i = 0; i + 1 < frame.size(); ++i) {
        decoder.append(std::string_view(frame).substr(i, 1));
        EXPECT_FALSE(decoder.nextFrame().has_value()) << "returned early at byte " << i;
    }
    decoder.append(std::string_view(frame).substr(frame.size() - 1));

    const auto payload = decoder.nextFrame();
    ASSERT_TRUE(payload.has_value());
    EXPECT_EQ(*payload, "split");
}

// One read can carry several messages glued together. Each must come out separately, in order.
TEST(FramingTest, SplitsSeveralFramesFromOneRead) {
    matchforge::net::FrameDecoder decoder(kLargeLimit);
    decoder.append(matchforge::net::encodeFrame("first") + matchforge::net::encodeFrame("second"));

    EXPECT_EQ(decoder.nextFrame().value_or(""), "first");
    EXPECT_EQ(decoder.nextFrame().value_or(""), "second");
    EXPECT_FALSE(decoder.nextFrame().has_value());
}

// A frame with an empty payload is still a valid frame: four zero bytes.
TEST(FramingTest, AcceptsAnEmptyPayload) {
    matchforge::net::FrameDecoder decoder(kLargeLimit);
    decoder.append(matchforge::net::encodeFrame(""));

    const auto payload = decoder.nextFrame();
    ASSERT_TRUE(payload.has_value());
    EXPECT_TRUE(payload->empty());
}

// A length above the limit must fail the decoder before the body arrives, so the
// server never buffers memory it would throw away.
TEST(FramingTest, RejectsAFrameLargerThanTheLimit) {
    matchforge::net::FrameDecoder decoder(/*maxPayloadBytes=*/8);
    // Header says 100 bytes. We send only the header, and the decoder should already refuse.
    decoder.append(std::string("\x00\x00\x00\x64", 4));

    EXPECT_FALSE(decoder.nextFrame().has_value());
    EXPECT_TRUE(decoder.failed());
}

// Once failed, the stream is untrusted. Later bytes must not produce frames.
TEST(FramingTest, StaysFailedAfterAnOversizedFrame) {
    matchforge::net::FrameDecoder decoder(/*maxPayloadBytes=*/8);
    decoder.append(std::string("\x00\x00\x00\x64", 4));
    ASSERT_FALSE(decoder.nextFrame().has_value());

    decoder.append(matchforge::net::encodeFrame("ok"));
    EXPECT_FALSE(decoder.nextFrame().has_value());
    EXPECT_TRUE(decoder.failed());
}

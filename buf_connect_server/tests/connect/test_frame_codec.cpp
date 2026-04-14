// FILE: buf_connect_server/tests/connect/test_frame_codec.cpp
#include "buf_connect_server/connect/frame_codec.hpp"
#include <gtest/gtest.h>

using namespace buf_connect_server::connect;

TEST(FrameCodecTest, RoundTripDataFrame) {
std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04};
auto encoded = EncodeFrame(std::span<const uint8_t>(payload), FrameFlag::kData);
ASSERT_EQ(encoded.size(), 9u);
EXPECT_EQ(encoded[0], 0x00);  // flag = kData
EXPECT_EQ(encoded[4], 0x04);  // length = 4
auto decoded = DecodeFrame(std::span<const uint8_t>(encoded));
EXPECT_EQ(decoded.bytes_consumed, 9u);
EXPECT_EQ(decoded.flag, FrameFlag::kData);
ASSERT_EQ(decoded.payload.size(), 4u);
EXPECT_EQ(decoded.payload[0], 0x01);
}

TEST(FrameCodecTest, RoundTripEndStreamFrame) {
std::vector<uint8_t> payload = {};
auto encoded = EncodeFrame(std::span<const uint8_t>(payload), FrameFlag::kEndStream);
ASSERT_EQ(encoded.size(), 5u);
EXPECT_EQ(encoded[0], 0x02);
auto decoded = DecodeFrame(std::span<const uint8_t>(encoded));
EXPECT_EQ(decoded.flag, FrameFlag::kEndStream);
EXPECT_EQ(decoded.payload.size(), 0u);
}

TEST(FrameCodecTest, TruncatedBufferReturnsZero) {
std::vector<uint8_t> short_buf = {0x00, 0x00};
auto decoded = DecodeFrame(std::span<const uint8_t>(short_buf));
EXPECT_EQ(decoded.bytes_consumed, 0u);
}

TEST(FrameCodecTest, InsufficientPayloadReturnsZero) {
std::vector<uint8_t> payload = {0x01, 0x02};
auto encoded = EncodeFrame(std::span<const uint8_t>(payload), FrameFlag::kData);
// Truncate to only the header
encoded.resize(5);
auto decoded = DecodeFrame(std::span<const uint8_t>(encoded));
EXPECT_EQ(decoded.bytes_consumed, 0u);
}

TEST(FrameCodecTest, LargePayload) {
std::vector<uint8_t> payload(65536, 0xAB);
auto encoded = EncodeFrame(std::span<const uint8_t>(payload), FrameFlag::kData);
auto decoded = DecodeFrame(std::span<const uint8_t>(encoded));
EXPECT_EQ(decoded.bytes_consumed, 65536u + 5u);
EXPECT_EQ(decoded.payload.size(), 65536u);
}

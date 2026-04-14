// FILE: buf_connect_server/include/buf_connect_server/connect/frame_codec.hpp
#ifndef BUF_CONNECT_SERVER_CONNECT_FRAME_CODEC_HPP
#define BUF_CONNECT_SERVER_CONNECT_FRAME_CODEC_HPP

#include <cstdint>
#include <span>
#include <vector>
#include <cstddef>

namespace buf_connect_server::connect {

enum class FrameFlag : uint8_t {
  kData      = 0x00,
  kEndStream = 0x02,
};

// Encodes payload into a 5-byte length-prefixed frame:
//   [1 byte flag][4 bytes big-endian length][payload bytes]
std::vector<uint8_t> EncodeFrame(std::span<const uint8_t> payload, FrameFlag flag);

struct DecodedFrame {
  FrameFlag                flag;
  std::span<const uint8_t> payload;
  size_t                   bytes_consumed;
};

// Decodes a 5-byte length-prefixed frame from buffer.
// Returns DecodedFrame with bytes_consumed == 0 if buffer is too short.
DecodedFrame DecodeFrame(std::span<const uint8_t> buffer);

}  // namespace buf_connect_server::connect

#endif  // BUF_CONNECT_SERVER_CONNECT_FRAME_CODEC_HPP

// FILE: buf_connect_server/src/connect/frame_codec.cpp
#include "buf_connect_server/connect/frame_codec.hpp"
#include <stdexcept>
#include <span>

std::vector<uint8_t> buf_connect_server::connect::EncodeFrame(
        std::span<const uint8_t> payload, FrameFlag flag) {
    std::vector<uint8_t> result;
    result.reserve(5 + payload.size());
    result.push_back(static_cast<uint8_t>(flag));
    uint32_t len = static_cast<uint32_t>(payload.size());
    result.push_back(static_cast<uint8_t>((len >> 24) & 0xFF));
    result.push_back(static_cast<uint8_t>((len >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((len >>  8) & 0xFF));
    result.push_back(static_cast<uint8_t>((len      ) & 0xFF));
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}

buf_connect_server::connect::DecodedFrame
buf_connect_server::connect::DecodeFrame(std::span<const uint8_t> buffer)
{
    if (buffer.size() < 5) {
        return DecodedFrame{FrameFlag::kData, {}, 0};
    }
    FrameFlag flag = static_cast<FrameFlag>(buffer[0]);
    uint32_t len = (static_cast<uint32_t>(buffer[1]) << 24)
                   | (static_cast<uint32_t>(buffer[2]) << 16)
                   | (static_cast<uint32_t>(buffer[3]) <<  8)
                   | (static_cast<uint32_t>(buffer[4]));
    if (buffer.size() < 5 + len) {
        return DecodedFrame{FrameFlag::kData, {}, 0};
    }
    return DecodedFrame{flag, buffer.subspan(5, len), 5 + len};
}

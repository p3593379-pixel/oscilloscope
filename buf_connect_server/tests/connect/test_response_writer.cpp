// FILE: buf_connect_server/tests/connect/test_response_writer.cpp
#include "buf_connect_server/connect/response_writer.hpp"
#include <gtest/gtest.h>
#include <vector>
#include <string>

using namespace buf_connect_server::connect;

struct WriterHarness {
    std::vector<std::vector<uint8_t>> writes;
    std::vector<std::pair<std::string,std::string>> headers;
    bool should_fail = false;

    ConnectResponseWriter Make(bool streaming) {
        WriteCallback wfn = [this](std::span<const uint8_t> data) -> bool {
            if (should_fail) return false;
            writes.emplace_back(data.begin(), data.end());
            return true;
        };
        HeaderCallback hfn = [this](const std::string& k, const std::string& v) {
            headers.emplace_back(k, v);
        };
        return ConnectResponseWriter(wfn, hfn, streaming);
    }
};

TEST(ResponseWriterTest, SendHeadersOnce) {
    WriterHarness h;
    auto w = h.Make(false);
    w.SendHeaders(200, "application/proto");
    w.SendHeaders(200, "application/proto");  // second call ignored
    EXPECT_EQ(h.headers.size(), 2u);  // :status + content-type
}

TEST(ResponseWriterTest, WriteUnaryDeliversPayload) {
    WriterHarness h;
    auto w = h.Make(false);
    w.SendHeaders(200, "application/proto");
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03};
    w.WriteUnary(std::span<const uint8_t>(payload));
    ASSERT_EQ(h.writes.size(), 1u);
    EXPECT_EQ(h.writes[0], payload);
}

TEST(ResponseWriterTest, WriteStreamingFrameHas5BytePrefix) {
    WriterHarness h;
    auto w = h.Make(true);
    w.SendHeaders(200, "application/connect+proto");
    std::vector<uint8_t> payload = {0xAA, 0xBB};
    w.WriteStreamingFrame(std::span<const uint8_t>(payload));
    ASSERT_EQ(h.writes.size(), 1u);
    auto& frame = h.writes[0];
    ASSERT_GE(frame.size(), 5u);
    EXPECT_EQ(frame[0], 0x00);   // FrameFlag::kData
    uint32_t len = (uint32_t(frame[1]) << 24) | (uint32_t(frame[2]) << 16)
                   | (uint32_t(frame[3]) <<  8) |  uint32_t(frame[4]);
    EXPECT_EQ(len, 2u);
    EXPECT_EQ(frame[5], 0xAA);
    EXPECT_EQ(frame[6], 0xBB);
}

TEST(ResponseWriterTest, WriteEndOfStreamUsesEndStreamFlag) {
    WriterHarness h;
    auto w = h.Make(true);
    w.SendHeaders(200, "application/connect+proto");
    w.WriteEndOfStream();
    ASSERT_EQ(h.writes.size(), 1u);
    EXPECT_EQ(h.writes[0][0], 0x02);  // FrameFlag::kEndStream
}

TEST(ResponseWriterTest, IsClientConnectedAfterWriteFailure) {
    WriterHarness h;
    h.should_fail = true;
    auto w = h.Make(false);
    w.SendHeaders(200, "application/proto");
    EXPECT_TRUE(w.IsClientConnected());  // before any write
    std::vector<uint8_t> p = {1};
    w.WriteUnary(std::span<const uint8_t>(p));
    EXPECT_FALSE(w.IsClientConnected());
}

TEST(ResponseWriterTest, SetDisconnectedStopsWrites) {
    WriterHarness h;
    auto w = h.Make(false);
    w.SendHeaders(200, "application/proto");
    w.SetDisconnected();
    std::vector<uint8_t> p = {1, 2};
    w.WriteUnary(std::span<const uint8_t>(p));
    EXPECT_TRUE(h.writes.empty());
}

TEST(ResponseWriterTest, WriteErrorProducesJson) {
    WriterHarness h;
    auto w = h.Make(false);
    w.SendHeaders(400, "application/json");
    w.WriteError("invalid_argument", "bad input");
    ASSERT_EQ(h.writes.size(), 1u);
    std::string body(h.writes[0].begin(), h.writes[0].end());
    EXPECT_NE(body.find("invalid_argument"), std::string::npos);
    EXPECT_NE(body.find("bad input"), std::string::npos);
}

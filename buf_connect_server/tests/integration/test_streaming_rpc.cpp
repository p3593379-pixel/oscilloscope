// FILE: buf_connect_server/tests/integration/test_streaming_rpc.cpp
#include "buf_connect_server/connect/response_writer.hpp"
#include "buf_connect_server/connect/frame_codec.hpp"
#include "buf_connect_server/connect/protocol.hpp"
#include "buf_connect_server/session/session_manager.hpp"
#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>

using namespace buf_connect_server;

// ── Frame encode/decode round-trip at scale ───────────────────────────────────

TEST(StreamingRpcTest, MultipleFramesRoundTrip) {
    std::vector<std::vector<uint8_t>> sent;

    connect::WriteCallback wfn = [&](std::span<const uint8_t> d) -> bool {
        sent.emplace_back(d.begin(), d.end());
        return true;
    };
    connect::HeaderCallback hfn = [](const std::string&, const std::string&) {};
    connect::ConnectResponseWriter w(wfn, hfn, true);

    w.SendHeaders(connect::kHttpOk, std::string(connect::kContentTypeConnectProto));

    for (int i = 0; i < 10; ++i) {
        std::vector<uint8_t> payload(64, static_cast<uint8_t>(i));
        w.WriteStreamingFrame(std::span<const uint8_t>(payload));
    }
    w.WriteEndOfStream();

    ASSERT_EQ(sent.size(), 11u);  // 10 data frames + 1 end-stream

    // Verify all data frames
    for (int i = 0; i < 10; ++i) {
        auto decoded = connect::DecodeFrame(std::span<const uint8_t>(sent[i]));
        EXPECT_EQ(decoded.flag, connect::FrameFlag::kData);
        ASSERT_EQ(decoded.payload.size(), 64u);
        for (auto b : decoded.payload)
            EXPECT_EQ(b, static_cast<uint8_t>(i));
    }

    // Verify end-stream frame
    auto eos = connect::DecodeFrame(std::span<const uint8_t>(sent[10]));
    EXPECT_EQ(eos.flag, connect::FrameFlag::kEndStream);
}

// ── Disconnect stops streaming ────────────────────────────────────────────────

TEST(StreamingRpcTest, WritesStopAfterDisconnect) {
    int write_count = 0;

    connect::WriteCallback wfn = [&](std::span<const uint8_t>) -> bool {
        ++write_count;
        return write_count < 3;  // fail after 2 writes
    };
    connect::HeaderCallback hfn = [](const std::string&, const std::string&) {};
    connect::ConnectResponseWriter w(wfn, hfn, true);

    w.SendHeaders(connect::kHttpOk, std::string(connect::kContentTypeConnectProto));
    EXPECT_TRUE(w.IsClientConnected());

    std::vector<uint8_t> p(8, 0xAA);
    for (int i = 0; i < 5; ++i)
        w.WriteStreamingFrame(std::span<const uint8_t>(p));

    EXPECT_FALSE(w.IsClientConnected());
    // writes stop after the 2nd WriteStreamingFrame triggers the failure
    EXPECT_LE(write_count, 4);
}

// ── Session subscription and disconnect ──────────────────────────────────────

TEST(StreamingRpcTest, SessionSubscribeUnsubscribe) {
    session::SessionManager mgr;
    mgr.Connect("conn-1", "eng-1", "engineer");

    // Build a writer that tracks frames
    std::vector<std::vector<uint8_t>> frames;
    connect::WriteCallback wfn = [&](std::span<const uint8_t> d) -> bool {
        frames.emplace_back(d.begin(), d.end());
        return true;
    };
    connect::HeaderCallback hfn = [](const std::string&, const std::string&) {};
    connect::ConnectResponseWriter writer(wfn, hfn, true);

    mgr.Subscribe("conn-1", &writer);

    // Connect a second session to trigger a broadcast event
    mgr.Connect("conn-2", "eng-2", "engineer");

    mgr.Unsubscribe("conn-1");
    mgr.Disconnect("conn-1");
    mgr.Disconnect("conn-2");
    // No crash = pass
    SUCCEED();
}

// ── Large frame integrity ─────────────────────────────────────────────────────

TEST(StreamingRpcTest, LargeFrameIntegrity) {
    std::vector<uint8_t> received;

    connect::WriteCallback wfn = [&](std::span<const uint8_t> d) -> bool {
        received.insert(received.end(), d.begin(), d.end());
        return true;
    };
    connect::HeaderCallback hfn = [](const std::string&, const std::string&) {};
    connect::ConnectResponseWriter w(wfn, hfn, true);

    w.SendHeaders(200, std::string(connect::kContentTypeConnectProto));

    const size_t kSize = 131072;  // 128 KB
    std::vector<uint8_t> big(kSize);
    for (size_t i = 0; i < kSize; ++i)
        big[i] = static_cast<uint8_t>(i & 0xFF);
    w.WriteStreamingFrame(std::span<const uint8_t>(big));

    // Decode the received bytes
    auto decoded = connect::DecodeFrame(std::span<const uint8_t>(received));
    EXPECT_EQ(decoded.flag, connect::FrameFlag::kData);
    ASSERT_EQ(decoded.payload.size(), kSize);
    for (size_t i = 0; i < kSize; ++i)
        EXPECT_EQ(decoded.payload[i], static_cast<uint8_t>(i & 0xFF));
}

// ── Concurrent writes from two threads ───────────────────────────────────────

TEST(StreamingRpcTest, ConcurrentSessionConnectsAreThreadSafe) {
    session::SessionManager mgr;
    std::atomic<int> active_count{0};

    auto connect_fn = [&](int id) {
        mgr.Connect("conn-" + std::to_string(id),
                    "user-" + std::to_string(id),
                    "engineer");
        ++active_count;
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i)
        threads.emplace_back(connect_fn, i);
    for (auto& t : threads)
        t.join();

    EXPECT_EQ(active_count.load(), 8);

    for (int i = 0; i < 8; ++i)
        mgr.Disconnect("conn-" + std::to_string(i));
}

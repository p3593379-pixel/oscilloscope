#include "services/oscilloscope_service_impl.hpp"
#include "buf_connect_server/auth/middleware.hpp"
#include "buf_connect_server/connect/protocol.hpp"
#include "buf_connect_server/auth/stream_token.hpp"
#include "buf_connect_server/connect/frame_codec.hpp"
#include "hardware/adc_reader.hpp"
#include "oscilloscope_interface.pb.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>
#include <mutex>
#include <utility>

OscilloscopeServiceImpl::OscilloscopeServiceImpl(std::string jwt_secret) : jwt_secret_(std::move(jwt_secret)) {};

std::string OscilloscopeServiceImpl::ServicePath() const {
    return "/oscilloscope_interface.v2.OscilloscopeService";
}

void OscilloscopeServiceImpl::RegisterRoutes(
        buf_connect_server::BufConnectServer& server) {
    namespace c = buf_connect_server::connect;

    // StreamData lives on the DATA plane — validated by stream token only
    server.RegisterDataRoute(
            "/oscilloscope_interface.v2.OscilloscopeService/StreamData",
            [this](const c::ParsedConnectRequest& req, c::ConnectResponseWriter& w) {
                HandleStreamData(req, w);
            });

    // Settings RPCs live on the CONTROL plane — require a valid JWT
    server.RegisterControlRoute(
            "/oscilloscope_interface.v2.OscilloscopeService/GetSettings",
            [this](const c::ParsedConnectRequest& req, c::ConnectResponseWriter& w) {
                HandleGetSettings(req, w);
            });
    server.RegisterControlRoute(
            "/oscilloscope_interface.v2.OscilloscopeService/UpdateSettings",
            [this](const c::ParsedConnectRequest& req, c::ConnectResponseWriter& w) {
                HandleUpdateSettings(req, w);
            });
}

// ─── StreamData ──────────────────────────────────────────────────────────────

void OscilloscopeServiceImpl::HandleStreamData(
        const buf_connect_server::connect::ParsedConnectRequest& req,
        buf_connect_server::connect::ConnectResponseWriter& writer) {
    namespace c = buf_connect_server::connect;

    // ── 1. Decode the Connect streaming frame wrapper ───────────────────────
    if (req.body.size() < 5) {
        writer.SendHeaders(c::kHttpBadRequest, "application/json");
        writer.WriteError(std::string(c::kCodeInvalidArgument), "missing frame");
        return;
    }
    auto decoded = c::DecodeFrame(std::span<const uint8_t>(req.body));
    if (decoded.bytes_consumed == 0) {
        writer.SendHeaders(c::kHttpBadRequest, "application/json");
        writer.WriteError(std::string(c::kCodeInvalidArgument), "malformed frame");
        return;
    }

    oscilloscope_interface::v2::StreamDataRequest stream_req;
    if (!stream_req.ParseFromArray(decoded.payload.data(),
                                   static_cast<int>(decoded.payload.size()))) {
        writer.SendHeaders(c::kHttpBadRequest, "application/json");
        writer.WriteError(std::string(c::kCodeInvalidArgument), "parse error");
        return;
    }

    // ── 2. Validate stream token (HMAC — no database lookup needed) ─────────
    // The stream_token field is set by the client from the token it received
    // via SessionService/GetStreamToken.
    {
        // We need the JWT master secret to derive the HMAC key.
        // We receive it at RegisterRoutes time — stored as member.
        buf_connect_server::auth::StreamToken st(jwt_secret_);
        auto claims = st.Validate(stream_req.stream_token());
        if (!claims) {
            writer.SendHeaders(c::kHttpUnauthorized, "application/json");
            writer.WriteError(std::string(c::kCodeUnauthenticated),
                              "invalid or expired stream token");
            return;
        }
        // Enforce decimation tier based on what was granted in the token
        if (claims->tier == "preview" &&
            stream_req.requested_tier() ==
            oscilloscope_interface::v2::DECIMATION_TIER_FULL) {
            spdlog::warn("Client requested FULL tier but token grants only PREVIEW");
            stream_req.set_requested_tier(
                    oscilloscope_interface::v2::DECIMATION_TIER_PREVIEW);
        }
    }

    // ── 3. Determine frame size for the granted tier ─────────────────────────
    auto tier = stream_req.requested_tier() ==
                oscilloscope_interface::v2::DECIMATION_TIER_FULL
                ? oscilloscope_interface::v2::DECIMATION_TIER_FULL
                : oscilloscope_interface::v2::DECIMATION_TIER_PREVIEW;

    uint32_t samples_per_frame;
    {
        std::lock_guard<std::mutex> lock(settings_mutex_);
        uint32_t frame_bytes = tier == oscilloscope_interface::v2::DECIMATION_TIER_FULL
                               ? 65536 : 8192;
        samples_per_frame = frame_bytes / channels_ / sizeof(int16_t);
    }

    writer.SendHeaders(c::kHttpOk, std::string(c::kContentTypeConnectProto));

    // ── 4. Stream loop ───────────────────────────────────────────────────────
    uint32_t sr, ch;
    {
        std::lock_guard<std::mutex> lock(settings_mutex_);
        sr = sample_rate_hz_;
        ch = channels_;
    }
    AdcReader adc(sr, ch);
    uint64_t  sequence = 0;

    while (writer.IsClientConnected()) {
        auto samples = adc.ReadChunk(samples_per_frame);

        oscilloscope_interface::v2::DataChunk chunk;
        chunk.set_samples(samples.data(), samples.size());
        chunk.set_sequence_number(sequence++);
        chunk.set_timestamp_ns(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count());
        chunk.set_tier(tier);
        {
            std::lock_guard<std::mutex> lock(settings_mutex_);
            chunk.set_sample_rate_hz(sample_rate_hz_);
            chunk.set_channels(channels_);
        }

        std::vector<uint8_t> out(chunk.ByteSizeLong());
        chunk.SerializeToArray(out.data(), static_cast<int>(out.size()));
        writer.WriteStreamingFrame(std::span<const uint8_t>(out));

        // Bandwidth throttling
        if (stream_req.max_bandwidth_mbps() > 0) {
            uint64_t delay_us = (static_cast<uint64_t>(out.size()) * 8ULL * 1'000'000ULL) /
                                (stream_req.max_bandwidth_mbps() * 1'000'000ULL);
            std::this_thread::sleep_for(std::chrono::microseconds(delay_us));
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    writer.WriteEndOfStream();
}

// ─── GetSettings ─────────────────────────────────────────────────────────────

void OscilloscopeServiceImpl::HandleGetSettings(
        const buf_connect_server::connect::ParsedConnectRequest&,
        buf_connect_server::connect::ConnectResponseWriter& writer) {
    namespace c = buf_connect_server::connect;

    oscilloscope_interface::v2::GetSettingsResponse resp;
    {
        std::lock_guard<std::mutex> lock(settings_mutex_);
        auto* s = resp.mutable_settings();
        s->set_sample_rate_hz(sample_rate_hz_);
        s->set_channels(channels_);
        s->set_voltage_range_mv(voltage_range_mv_);
        s->set_trigger_level_mv(trigger_level_mv_);
        s->set_trigger_enabled(trigger_enabled_);
    }
    std::vector<uint8_t> out(resp.ByteSizeLong());
    resp.SerializeToArray(out.data(), static_cast<int>(out.size()));
    writer.SendHeaders(c::kHttpOk, std::string(c::kContentTypeProto));
    writer.WriteUnary(std::span<const uint8_t>(out));
}

// ─── UpdateSettings ──────────────────────────────────────────────────────────

void OscilloscopeServiceImpl::HandleUpdateSettings(
        const buf_connect_server::connect::ParsedConnectRequest& req,
        buf_connect_server::connect::ConnectResponseWriter& writer) {
    namespace c = buf_connect_server::connect;

    // Unary: try frame-decode, fall back to raw proto
    std::vector<uint8_t> body = req.body;
    if (body.size() >= 5) {
        auto d = c::DecodeFrame(std::span<const uint8_t>(body));
        if (d.bytes_consumed > 0)
            body = {d.payload.begin(), d.payload.end()};
    }

    oscilloscope_interface::v2::UpdateSettingsRequest update_req;
    if (!update_req.ParseFromArray(body.data(), static_cast<int>(body.size()))) {
        writer.SendHeaders(c::kHttpBadRequest, "application/json");
        writer.WriteError(std::string(c::kCodeInvalidArgument), "parse error");
        return;
    }

    const auto& s = update_req.settings();
    {
        std::lock_guard<std::mutex> lock(settings_mutex_);
        sample_rate_hz_   = s.sample_rate_hz();
        channels_         = s.channels();
        voltage_range_mv_ = s.voltage_range_mv();
        trigger_level_mv_ = s.trigger_level_mv();
        trigger_enabled_  = s.trigger_enabled();
    }

    oscilloscope_interface::v2::UpdateSettingsResponse resp;
    *resp.mutable_settings() = s;
    std::vector<uint8_t> out(resp.ByteSizeLong());
    resp.SerializeToArray(out.data(), static_cast<int>(out.size()));
    writer.SendHeaders(c::kHttpOk, std::string(c::kContentTypeProto));
    writer.WriteUnary(std::span<const uint8_t>(out));
}

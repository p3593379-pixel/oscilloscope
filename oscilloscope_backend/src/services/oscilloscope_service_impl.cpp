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

OscilloscopeServiceImpl::OscilloscopeServiceImpl(std::string jwt_secret)
        : stream_token_(jwt_secret) {}

std::string OscilloscopeServiceImpl::ServicePath() const {
    return "/oscilloscope_interface.v2.OscilloscopeService";
}

void OscilloscopeServiceImpl::RegisterRoutes(
        buf_connect_server::BufConnectServer& server) {
    namespace c = buf_connect_server::connect;

    server.RegisterDataRoute(
            "/oscilloscope_interface.v2.OscilloscopeService/StreamData",
            [this](const c::ParsedConnectRequest& req, c::ConnectResponseWriter& w) {
                HandleStreamData(req, w);
            });

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

void OscilloscopeServiceImpl::HandleStreamData(const buf_connect_server::connect::ParsedConnectRequest& req,
        buf_connect_server::connect::ConnectResponseWriter& writer)
{
    namespace c = buf_connect_server::connect;
    namespace a = buf_connect_server::auth;

    // ── 1. Decode Connect streaming frame wrapper ────────────────────────────
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

    // ── 2. Validate stream token ─────────────────────────────────────────────
    auto claims = stream_token_.Validate(stream_req.stream_token());
    if (!claims) {
        writer.SendHeaders(c::kHttpUnauthorized, "application/json");
        writer.WriteError(std::string(c::kCodeUnauthenticated),
                          "invalid or expired stream token");
        return;
    }
    if (claims->decimation_rate > 1 &&
        stream_req.requested_tier() == oscilloscope_interface::v2::DECIMATION_TIER_FULL) {
        spdlog::warn("Client requested FULL tier but token grants only PREVIEW");
        stream_req.set_requested_tier(
                oscilloscope_interface::v2::DECIMATION_TIER_PREVIEW);
    }

    // ── 3. Resolve streaming parameters ─────────────────────────────────────
    //   frame_size  — samples per channel per frame (client hint, or default 8192)
    //   target_fps  — frames per second             (client hint, or default 30)
    const uint32_t samples_per_frame =
            stream_req.frame_size() > 0 ? stream_req.frame_size() : 8192u;

    const uint32_t target_fps =
            stream_req.target_fps() > 0 ? stream_req.target_fps() : 30u;

    const auto frame_period =
            std::chrono::microseconds(1'000'000u / target_fps);

    const auto tier = stream_req.requested_tier() ==
                      oscilloscope_interface::v2::DECIMATION_TIER_FULL
                      ? oscilloscope_interface::v2::DECIMATION_TIER_FULL
                      : oscilloscope_interface::v2::DECIMATION_TIER_PREVIEW;

    spdlog::debug("[StreamData] frame_size={} fps={}", samples_per_frame, target_fps);

    // ── 4. Start streaming ───────────────────────────────────────────────────
    writer.SendHeaders(c::kHttpOk, std::string(c::kContentTypeConnectProto));

    uint32_t sr, ch;
    {
        std::lock_guard<std::mutex> lock(settings_mutex_);
        sr = sample_rate_hz_;
        ch = channels_;
    }
    AdcReader adc(sr, ch);
    uint64_t  sequence = 0;

    while (writer.IsClientConnected()) {
        const auto frame_start = std::chrono::steady_clock::now();

        // Acquire one frame of interleaved float32 samples
        const auto raw_samples = adc.ReadChunk(samples_per_frame);

        // Build DataChunk
        oscilloscope_interface::v2::DataChunk chunk;
        chunk.set_samples(raw_samples.data(), raw_samples.size());
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
        SPDLOG_INFO("Sent frame; size {}", out.size());
        // ── FPS throttle ─────────────────────────────────────────────────────
        const auto elapsed = std::chrono::steady_clock::now() - frame_start;
        if (elapsed < frame_period)
            std::this_thread::sleep_for(frame_period - elapsed);
    }
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

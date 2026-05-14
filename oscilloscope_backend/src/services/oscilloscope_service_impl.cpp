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
    server.RegisterDataRoute(
            "/oscilloscope_interface.v2.OscilloscopeService/StreamSpectrogram",
            [this](const c::ParsedConnectRequest& req, c::ConnectResponseWriter& w) {
                HandleStreamSpectrogram(req, w);
            });

    server.RegisterControlRoute(
            "/oscilloscope_interface.v2.OscilloscopeService/GetSettings",
            [this](const c::ParsedConnectRequest& req, c::ConnectResponseWriter& w) {
                HandleGetSettings(req, w);
            });
    server.RegisterControlRoute(
            "/oscilloscope_interface.v2.OscilloscopeService/SetSettings",
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
    writer.WriteEndOfStream();  // Send Connect end-stream trailer
}

// ─── StreamSpectrogram ────────────────────────────────────────────────────────
//
// Produces SpectrogramChunk frames at ~10 fps (or client-requested rate).
// Each frame contains `channels * (fft_size/2 + 1)` float32 dBFS magnitudes,
// interleaved by bin:  BIN0_CH0, BIN0_CH1, BIN1_CH0, BIN1_CH1, ...
//
// The DFT is computed as a naive O(N²) DFT — perfectly adequate for a stub
// with fft_size ≤ 1024, and avoids adding a kissfft/pocketfft dependency.
// Swap in a real FFT library when connecting real hardware.

namespace {

// Hann window weight for sample index i out of N
    inline float hann(uint32_t i, uint32_t N) {
        return 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(std::numbers::pi)
                                       * static_cast<float>(i) / static_cast<float>(N - 1)));
    }

// Compute magnitude spectrum (dBFS) for one channel.
// `samples` — time-domain samples (length >= fft_size).
// Returns `fft_size/2 + 1` magnitude values in dBFS.
    std::vector<float> ComputeMagnitudeDb(const float* samples,
                                          uint32_t     fft_size) {
        const uint32_t bins = fft_size / 2 + 1;
        std::vector<float> out(bins);

        for (uint32_t k = 0; k < bins; ++k) {
            double re = 0.0, im = 0.0;
            for (uint32_t n = 0; n < fft_size; ++n) {
                const double angle = -2.0 * std::numbers::pi * k * n / fft_size;
                const double w     = hann(n, fft_size);
                re += w * samples[n] * std::cos(angle);
                im += w * samples[n] * std::sin(angle);
            }
            // Normalise by fft_size/2 so a full-scale sine gives 0 dBFS
            const double mag = std::sqrt(re * re + im * im) / (fft_size / 2.0);
            out[k] = static_cast<float>(20.0 * std::log10(mag + 1e-9));
        }
        return out;
    }

} // namespace

void OscilloscopeServiceImpl::HandleStreamSpectrogram(
        const buf_connect_server::connect::ParsedConnectRequest& req,
        buf_connect_server::connect::ConnectResponseWriter& writer)
{
    namespace c = buf_connect_server::connect;

    // ── 1. Decode frame wrapper ───────────────────────────────────────────────
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

    oscilloscope_interface::v2::StreamSpectrogramRequest spec_req;
    if (!spec_req.ParseFromArray(decoded.payload.data(),
                                 static_cast<int>(decoded.payload.size()))) {
        writer.SendHeaders(c::kHttpBadRequest, "application/json");
        writer.WriteError(std::string(c::kCodeInvalidArgument), "parse error");
        return;
    }

    // ── 2. Validate stream token (same policy as StreamData) ─────────────────
    auto claims = stream_token_.ValidateTyped(spec_req.spectrogram_token(), "spectrogram_stream");
    if (!claims) {
        writer.SendHeaders(c::kHttpUnauthorized, "application/json");
        writer.WriteError(std::string(c::kCodeUnauthenticated),
                          "invalid or expired stream token");
        return;
    }

    // ── 3. Resolve parameters ─────────────────────────────────────────────────
    const uint32_t fft_size   = spec_req.fft_size()   > 0 ? spec_req.fft_size()   : 512u;
    const uint32_t target_fps = spec_req.target_fps() > 0 ? spec_req.target_fps() : 10u;
    const auto     frame_period = std::chrono::microseconds(1'000'000u / target_fps);

    spdlog::debug("[StreamSpectrogram] fft_size={} fps={}", fft_size, target_fps);

    writer.SendHeaders(c::kHttpOk, std::string(c::kContentTypeConnectProto));

    uint32_t sr, ch;
    {
        std::lock_guard<std::mutex> lock(settings_mutex_);
        sr = sample_rate_hz_;
        ch = channels_;
    }
    SPDLOG_CRITICAL("NUMCHANNELS: {}", ch);
    if (ch == 0) {
        writer.WriteError(std::string(c::kCodeInternal), "no channels configured");
        return;
    }
    // We only need fft_size samples per channel, so request exactly that.
    AdcReader adc(sr, ch);
    uint64_t  sequence  = 0;
    const uint32_t bins = fft_size / 2 + 1;

    while (writer.IsClientConnected()) {
        const auto frame_start = std::chrono::steady_clock::now();

        // Acquire exactly fft_size samples per channel
        const auto raw = adc.ReadChunk(fft_size);
        // raw is interleaved: [s0_ch0, s0_ch1, s1_ch0, s1_ch1, ...]
        const auto* raw_f32 = reinterpret_cast<const float*>(raw.data());

        // De-interleave and compute per-channel spectra
        // Output interleaving: BIN0_CH0, BIN0_CH1, BIN1_CH0, BIN1_CH1, ...
        std::vector<float> time_ch(fft_size);
        std::vector<std::vector<float>> spectra(ch);

        for (uint32_t c_idx = 0; c_idx < ch; ++c_idx) {
            for (uint32_t i = 0; i < fft_size; ++i)
                time_ch[i] = raw_f32[i * ch + c_idx];
            spectra[c_idx] = ComputeMagnitudeDb(time_ch.data(), fft_size);
        }

        // Interleave spectra into the output bytes buffer
        std::vector<float> interleaved(bins * ch);
        for (uint32_t bin = 0; bin < bins; ++bin)
            for (uint32_t c_idx = 0; c_idx < ch; ++c_idx)
                interleaved[bin * ch + c_idx] = spectra[c_idx][bin];

        // Build SpectrogramChunk
        oscilloscope_interface::v2::SpectrogramChunk chunk;
        chunk.set_magnitudes_db(
                reinterpret_cast<const char*>(interleaved.data()),
                interleaved.size() * sizeof(float));
        chunk.set_sequence_number(sequence++);
        chunk.set_timestamp_ns(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count());
        chunk.set_fft_size(fft_size);
        chunk.set_freq_resolution_hz(static_cast<float>(sr) / static_cast<float>(fft_size));
        {
            std::lock_guard<std::mutex> lock(settings_mutex_);
            chunk.set_sample_rate_hz(sample_rate_hz_);
            chunk.set_channels(channels_);
        }

        std::vector<uint8_t> out(chunk.ByteSizeLong());
        chunk.SerializeToArray(out.data(), static_cast<int>(out.size()));
        writer.WriteStreamingFrame(std::span<const uint8_t>(out));
        SPDLOG_INFO("Sent spec frame; size {}", out.size());

        const auto elapsed = std::chrono::steady_clock::now() - frame_start;
        if (elapsed < frame_period)
            std::this_thread::sleep_for(frame_period - elapsed);
    }
    writer.WriteEndOfStream();  // Send Connect end-stream trailer
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
//        s->set_channels(channels_);
//        s->set_voltage_range_mv(voltage_range_mv_);
//        s->set_trigger_level_mv(trigger_level_mv_);
//        s->set_trigger_enabled(trigger_enabled_);
    }
    std::vector<uint8_t> out(resp.ByteSizeLong());
    resp.SerializeToArray(out.data(), static_cast<int>(out.size()));
    writer.SendHeaders(c::kHttpOk, std::string(c::kContentTypeProto));
    writer.WriteUnary(std::span<const uint8_t>(out));
}

// ─── SetSettings ──────────────────────────────────────────────────────────

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

    oscilloscope_interface::v2::SetSettingsRequest update_req;
    if (!update_req.ParseFromArray(body.data(), static_cast<int>(body.size()))) {
        writer.SendHeaders(c::kHttpBadRequest, "application/json");
        writer.WriteError(std::string(c::kCodeInvalidArgument), "parse error");
        return;
    }

    const auto& s = update_req.settings();
    {
        std::lock_guard<std::mutex> lock(settings_mutex_);
        sample_rate_hz_   = s.sample_rate_hz();
//        channels_         = s.channels();
//        voltage_range_mv_ = s.voltage_range_mv();
//        trigger_level_mv_ = s.trigger_level_mv();
//        trigger_enabled_  = s.trigger_enabled();
    }

    oscilloscope_interface::v2::SetSettingsResponse resp;
    *resp.mutable_settings() = s;
    std::vector<uint8_t> out(resp.ByteSizeLong());
    resp.SerializeToArray(out.data(), static_cast<int>(out.size()));
    writer.SendHeaders(c::kHttpOk, std::string(c::kContentTypeProto));
    writer.WriteUnary(std::span<const uint8_t>(out));
}

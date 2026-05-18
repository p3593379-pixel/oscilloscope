// FILE: oscilloscope_backend/src/services/oscilloscope_service_impl.cpp
#include "services/oscilloscope_service_impl.hpp"
#include "buf_connect_server/connect/protocol.hpp"
#include "buf_connect_server/connect/frame_codec.hpp"
#include "hardware/adc_reader.hpp"
#include "oscilloscope_interface.pb.h"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <thread>
#include <fstream>
#include <filesystem>
#include <numbers>
#include <cmath>
#include <utility>
#include <vector>
#include <string>
#include <algorithm>

using json = nlohmann::json;
namespace c = buf_connect_server::connect;

// ═══════════════════════════════════════════════════════════════════════════════
// Construction / route registration
// ═══════════════════════════════════════════════════════════════════════════════

OscilloscopeServiceImpl::OscilloscopeServiceImpl(std::string jwt_secret,
                                                 std::string settings_path)
        : stream_token_(std::move(jwt_secret))
        , settings_path_(std::move(settings_path))
{
    LoadSettings();
}

std::string OscilloscopeServiceImpl::ServicePath() const {
    return "/oscilloscope_interface.v2.OscilloscopeService";
}

void OscilloscopeServiceImpl::RegisterRoutes(buf_connect_server::BufConnectServer& server) {
    server.RegisterDataRoute(
            "/oscilloscope_interface.v2.OscilloscopeService/StreamData",
            [this](const c::ParsedConnectRequest& req, c::ConnectResponseWriter& w) {
                HandleStreamData(req, w); });

    server.RegisterDataRoute(
            "/oscilloscope_interface.v2.OscilloscopeService/StreamSpectrogram",
            [this](const c::ParsedConnectRequest& req, c::ConnectResponseWriter& w) {
                HandleStreamSpectrogram(req, w); });

    server.RegisterDataRoute(
            "/oscilloscope_interface.v2.OscilloscopeService/GetSettings",
            [this](const c::ParsedConnectRequest& req, c::ConnectResponseWriter& w) {
                HandleGetSettings(req, w); });

    server.RegisterDataRoute(
            "/oscilloscope_interface.v2.OscilloscopeService/SetSettings",
            [this](const c::ParsedConnectRequest& req, c::ConnectResponseWriter& w) {
                HandleSetSettings(req, w); });

    server.RegisterDataRoute(
            "/oscilloscope_interface.v2.OscilloscopeService/BrowseDirectory",
            [this](const c::ParsedConnectRequest& req, c::ConnectResponseWriter& w) {
                HandleBrowseDirectory(req, w); });
}

// ═══════════════════════════════════════════════════════════════════════════════
// Persistence
// ═══════════════════════════════════════════════════════════════════════════════

void OscilloscopeServiceImpl::LoadSettings() {
    if (!std::filesystem::exists(settings_path_)) {
        spdlog::info("[OscService] '{}' not found — writing defaults", settings_path_);
        SaveSettings();
        return;
    }
    try {
        std::ifstream f(settings_path_);
        const json j = json::parse(f);
        std::lock_guard<std::mutex> lk(settings_mutex_);
        auto& s = settings_;

        s.sample_rate_hz       = j.value("sample_rate_hz",       s.sample_rate_hz);
        s.frame_size_samples   = j.value("frame_size_samples",   s.frame_size_samples);
        s.frame_frequency_hz   = j.value("frame_frequency_hz",   s.frame_frequency_hz);
        s.display_frequency_hz = j.value("display_frequency_hz", s.display_frequency_hz);
        s.decimation_rate      = j.value("decimation_rate",      s.decimation_rate);
        s.channels             = j.value("channels",             s.channels);
        s.spectrogram_fft_size = j.value("spectrogram_fft_size", s.spectrogram_fft_size);
        s.spectrogram_fps      = j.value("spectrogram_fps",      s.spectrogram_fps);
        s.interpolation        = j.value("interpolation",        s.interpolation);
        s.persistence_enabled  = j.value("persistence_enabled",  s.persistence_enabled);
        s.persistence_decay    = j.value("persistence_decay",    s.persistence_decay);
        s.grid_opacity         = j.value("grid_opacity",         s.grid_opacity);
        s.display_theme        = j.value("display_theme",        s.display_theme);
        s.natural_units        = j.value("natural_units",        s.natural_units);
        s.trigger_mode         = j.value("trigger_mode",         s.trigger_mode);
        s.trigger_edge         = j.value("trigger_edge",         s.trigger_edge);
        s.trigger_level_v      = j.value("trigger_level_v",      s.trigger_level_v);
        s.trigger_channel      = j.value("trigger_channel",      s.trigger_channel);

        // ── New fields ──────────────────────────────────────────────────────
        s.daq_mode             = j.value("daq_mode",             s.daq_mode);
        s.pre_delay_samples    = j.value("pre_delay_samples",    s.pre_delay_samples);
        s.spec_setting_one     = j.value("spec_setting_one",     s.spec_setting_one);
        s.spec_setting_two     = j.value("spec_setting_two",     s.spec_setting_two);
        s.spec_setting_three   = j.value("spec_setting_three",   s.spec_setting_three);
        s.spec_setting_four    = j.value("spec_setting_four",    s.spec_setting_four);
        s.write_adc                = j.value("write_adc",                s.write_adc);
        s.adc_archive_path         = j.value("adc_archive_path",         s.adc_archive_path);
        s.write_spectrogram        = j.value("write_spectrogram",        s.write_spectrogram);
        s.spectrogram_archive_path = j.value("spectrogram_archive_path", s.spectrogram_archive_path);

        if (j.contains("channels_cfg")) {
            const auto& cc = j["channels_cfg"];
            for (uint32_t i = 0; i < 4 && i < cc.size(); ++i) {
                s.ch_enabled[i]         = cc[i].value("enabled",         s.ch_enabled[i]);
                s.ch_volts_per_div[i]   = cc[i].value("volts_per_div",   s.ch_volts_per_div[i]);
                s.ch_vertical_offset[i] = cc[i].value("vertical_offset", s.ch_vertical_offset[i]);
            }
        }
        spdlog::info("[OscService] settings loaded from '{}'", settings_path_);
    } catch (const std::exception& e) {
        spdlog::warn("[OscService] load failed ({}), using defaults", e.what());
    }
}

void OscilloscopeServiceImpl::SaveSettings() const {
    OscServiceSettings snap;
    { std::lock_guard<std::mutex> lk(settings_mutex_); snap = settings_; }
    try {
        json j;
        j["sample_rate_hz"]       = snap.sample_rate_hz;
        j["frame_size_samples"]   = snap.frame_size_samples;
        j["frame_frequency_hz"]   = snap.frame_frequency_hz;
        j["display_frequency_hz"] = snap.display_frequency_hz;
        j["decimation_rate"]      = snap.decimation_rate;
        j["channels"]             = snap.channels;
        j["spectrogram_fft_size"] = snap.spectrogram_fft_size;
        j["spectrogram_fps"]      = snap.spectrogram_fps;
        j["interpolation"]        = snap.interpolation;
        j["persistence_enabled"]  = snap.persistence_enabled;
        j["persistence_decay"]    = snap.persistence_decay;
        j["grid_opacity"]         = snap.grid_opacity;
        j["display_theme"]        = snap.display_theme;
        j["natural_units"]        = snap.natural_units;
        j["trigger_mode"]         = snap.trigger_mode;
        j["trigger_edge"]         = snap.trigger_edge;
        j["trigger_level_v"]      = snap.trigger_level_v;
        j["trigger_channel"]      = snap.trigger_channel;

        // ── New fields ──────────────────────────────────────────────────────
        j["daq_mode"]             = snap.daq_mode;
        j["pre_delay_samples"]    = snap.pre_delay_samples;
        j["spec_setting_one"]     = snap.spec_setting_one;
        j["spec_setting_two"]     = snap.spec_setting_two;
        j["spec_setting_three"]   = snap.spec_setting_three;
        j["spec_setting_four"]    = snap.spec_setting_four;
        j["write_adc"]                = snap.write_adc;
        j["adc_archive_path"]         = snap.adc_archive_path;
        j["write_spectrogram"]        = snap.write_spectrogram;
        j["spectrogram_archive_path"] = snap.spectrogram_archive_path;

        json cc = json::array();
        for (uint32_t i = 0; i < 4; ++i)
            cc.push_back({ {"enabled",         snap.ch_enabled[i]},
                           {"volts_per_div",   snap.ch_volts_per_div[i]},
                           {"vertical_offset", snap.ch_vertical_offset[i]} });
        j["channels_cfg"] = cc;

        std::ofstream f(settings_path_);
        f << j.dump(2);
        spdlog::info("[OscService] settings saved to '{}'", settings_path_);
    } catch (const std::exception& e) {
        spdlog::error("[OscService] save failed: {}", e.what());
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Proto ↔ struct helpers
// ═══════════════════════════════════════════════════════════════════════════════

void OscilloscopeServiceImpl::FillProto(
        oscilloscope_interface::v2::OscilloscopeSettings& out) const
{
    const auto& s = settings_;  // caller holds settings_mutex_
    out.set_sample_rate_hz(s.sample_rate_hz);
    out.set_frame_size_samples(s.frame_size_samples);
    out.set_frame_frequency_hz(s.frame_frequency_hz);
    out.set_display_frequency_hz(s.display_frequency_hz);
    out.set_decimation_rate(s.decimation_rate);
    out.set_spectrogram_fft_size(s.spectrogram_fft_size);
    out.set_spectrogram_fps(s.spectrogram_fps);
    out.set_interpolation(
            static_cast<oscilloscope_interface::v2::Interpolation>(s.interpolation));
    out.set_persistence_enabled(s.persistence_enabled);
    out.set_persistence_decay(s.persistence_decay);
    out.set_natural_units(s.natural_units);
    out.set_display_theme(s.display_theme);
    out.set_grid_opacity(s.grid_opacity);

    auto* trig = out.mutable_trigger();
    trig->set_mode(static_cast<oscilloscope_interface::v2::TriggerMode>(s.trigger_mode));
    trig->set_edge(static_cast<oscilloscope_interface::v2::TriggerEdge>(s.trigger_edge));
    trig->set_level_v(s.trigger_level_v);
    trig->set_channel(s.trigger_channel);

    for (uint32_t i = 0; i < s.channels; ++i) {
        auto* ch = out.add_channels();
        ch->set_enabled(s.ch_enabled[i]);
        ch->set_volts_per_div(s.ch_volts_per_div[i]);
        ch->set_vertical_offset(s.ch_vertical_offset[i]);
    }

    // ── New fields ──────────────────────────────────────────────────────────
    out.set_daq_mode(
            static_cast<oscilloscope_interface::v2::DaqMode>(s.daq_mode));
    out.set_pre_delay_samples(s.pre_delay_samples);
    out.set_spec_setting_one(s.spec_setting_one);
    out.set_spec_setting_two(s.spec_setting_two);
    out.set_spec_setting_three(s.spec_setting_three);
    out.set_spec_setting_four(s.spec_setting_four);
    out.set_write_adc(s.write_adc);
    out.set_adc_archive_path(s.adc_archive_path);
    out.set_write_spectrogram(s.write_spectrogram);
    out.set_spectrogram_archive_path(s.spectrogram_archive_path);
}

bool OscilloscopeServiceImpl::ApplyProto(
        const oscilloscope_interface::v2::OscilloscopeSettings& proto,
        const google::protobuf::FieldMask& mask)
{
    auto& s = settings_;
    bool pipeline_changed = false;

    auto has = [&](const std::string& path) -> bool {
        if (mask.paths().empty()) return true;
        for (const auto& p : mask.paths()) if (p == path) return true;
        return false;
    };

#define APPLY_P(field, getter) \
    if (has(#field) && proto.getter() != 0) { \
        pipeline_changed |= (s.field != static_cast<decltype(s.field)>(proto.getter())); \
        s.field = static_cast<decltype(s.field)>(proto.getter()); }

    APPLY_P(sample_rate_hz,       sample_rate_hz)
    APPLY_P(frame_size_samples,   frame_size_samples)
    APPLY_P(frame_frequency_hz,   frame_frequency_hz)
    APPLY_P(display_frequency_hz, display_frequency_hz)
    APPLY_P(decimation_rate,      decimation_rate)
#undef APPLY_P

    if (has("spectrogram_fft_size") && proto.spectrogram_fft_size() != 0)
        s.spectrogram_fft_size = proto.spectrogram_fft_size();
    if (has("spectrogram_fps") && proto.spectrogram_fps() != 0)
        s.spectrogram_fps = proto.spectrogram_fps();
    if (has("interpolation") && proto.interpolation() != 0)
        s.interpolation = static_cast<uint32_t>(proto.interpolation());
    if (has("persistence_enabled"))
        s.persistence_enabled = proto.persistence_enabled();
    if (has("persistence_decay"))
        s.persistence_decay = proto.persistence_decay();
    if (has("natural_units"))
        s.natural_units = proto.natural_units();
    if (has("display_theme"))
        s.display_theme = proto.display_theme();
    if (has("grid_opacity"))
        s.grid_opacity = proto.grid_opacity();
    if (has("trigger") && proto.has_trigger()) {
        const auto& t = proto.trigger();
        if (t.mode())             s.trigger_mode    = static_cast<uint32_t>(t.mode());
        if (t.edge())             s.trigger_edge    = static_cast<uint32_t>(t.edge());
        if (t.level_v() != 0.0f) s.trigger_level_v = t.level_v();
        if (t.channel() != 0)    s.trigger_channel  = t.channel();
    }
    if (has("channels")) {
        for (int i = 0; i < proto.channels_size() && i < 4; ++i) {
            const auto& ch          = proto.channels(i);
            s.ch_enabled[i]         = ch.enabled();
            s.ch_volts_per_div[i]   = ch.volts_per_div();
            s.ch_vertical_offset[i] = ch.vertical_offset();
        }
    }

    // ── New fields ──────────────────────────────────────────────────────────
    if (has("daq_mode") && proto.daq_mode() != 0) {
        pipeline_changed |= (s.daq_mode != static_cast<uint32_t>(proto.daq_mode()));
        s.daq_mode = static_cast<uint32_t>(proto.daq_mode());
    }
    if (has("pre_delay_samples"))
        s.pre_delay_samples = proto.pre_delay_samples();
    if (has("spec_setting_one"))
        s.spec_setting_one = proto.spec_setting_one();
    if (has("spec_setting_two"))
        s.spec_setting_two = proto.spec_setting_two();
    if (has("spec_setting_three"))
        s.spec_setting_three = proto.spec_setting_three();
    if (has("spec_setting_four"))
        s.spec_setting_four = proto.spec_setting_four();
    if (has("write_adc"))
        s.write_adc = proto.write_adc();
    if (has("adc_archive_path") && !proto.adc_archive_path().empty())
        s.adc_archive_path = proto.adc_archive_path();
    if (has("write_spectrogram"))
        s.write_spectrogram = proto.write_spectrogram();
    if (has("spectrogram_archive_path") && !proto.spectrogram_archive_path().empty())
        s.spectrogram_archive_path = proto.spectrogram_archive_path();

    return pipeline_changed;
}

// ═══════════════════════════════════════════════════════════════════════════════
// BrowseDirectory
// ═══════════════════════════════════════════════════════════════════════════════

void OscilloscopeServiceImpl::HandleBrowseDirectory(
        const c::ParsedConnectRequest& req,
        c::ConnectResponseWriter& writer)
{
    // Decode Connect frame wrapper
    std::vector<uint8_t> body = req.body;
    if (body.size() >= 5) {
        auto d = c::DecodeFrame(std::span<const uint8_t>(body));
        if (d.bytes_consumed > 0)
            body = { d.payload.begin(), d.payload.end() };
    }

    oscilloscope_interface::v2::BrowseDirectoryRequest browse_req;
    if (!browse_req.ParseFromArray(body.data(), static_cast<int>(body.size()))) {
        writer.SendHeaders(c::kHttpBadRequest, "application/json");
        writer.WriteError(std::string(c::kCodeInvalidArgument), "parse error");
        return;
    }

    std::string path = browse_req.path().empty() ? "/" : browse_req.path();

    // Normalise: strip trailing slash except for root
    if (path.size() > 1 && path.back() == '/')
        path.pop_back();

    oscilloscope_interface::v2::BrowseDirectoryResponse resp;
    resp.set_current_path(path);

    // Parent path
    if (path != "/") {
        auto pos = path.rfind('/');
        resp.set_parent_path(pos == 0 ? "/" : path.substr(0, pos));
    }

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
        // Skip hidden files and entries we can't stat
        std::string name = entry.path().filename().string();
        if (name.empty() || name[0] == '.') continue;

        std::error_code ec2;
        bool is_dir = entry.is_directory(ec2);
        if (ec2) continue;   // permission error — skip

        auto* e = resp.add_entries();
        e->set_name(name);
        e->set_is_dir(is_dir);
    }
    if (ec) {
        writer.SendHeaders(c::kHttpBadRequest, "application/json");
        writer.WriteError(std::string(c::kCodeInvalidArgument),
                          "cannot read directory: " + ec.message());
        return;
    }

    // Sort: directories first, then files, both alpha
    auto* entries = resp.mutable_entries();
    std::sort(entries->begin(), entries->end(),
              [](const auto& a, const auto& b) {
                  if (a.is_dir() != b.is_dir()) return a.is_dir() > b.is_dir();
                  return a.name() < b.name();
              });

    std::vector<uint8_t> wire(resp.ByteSizeLong());
    resp.SerializeToArray(wire.data(), static_cast<int>(wire.size()));
    writer.SendHeaders(c::kHttpOk, std::string(c::kContentTypeProto));
    writer.WriteUnary(std::span<const uint8_t>(wire));
}

// ═══════════════════════════════════════════════════════════════════════════════
// GetSettings / SetSettings — unchanged logic, comment updated
// ═══════════════════════════════════════════════════════════════════════════════

void OscilloscopeServiceImpl::HandleGetSettings(
        const c::ParsedConnectRequest&,
        c::ConnectResponseWriter& writer)
{
    oscilloscope_interface::v2::GetSettingsResponse resp;
    {
        std::lock_guard<std::mutex> lk(settings_mutex_);
        FillProto(*resp.mutable_settings());
    }
    std::vector<uint8_t> wire(resp.ByteSizeLong());
    resp.SerializeToArray(wire.data(), static_cast<int>(wire.size()));
    writer.SendHeaders(c::kHttpOk, std::string(c::kContentTypeProto));
    writer.WriteUnary(std::span<const uint8_t>(wire));
}

void OscilloscopeServiceImpl::HandleSetSettings(
        const c::ParsedConnectRequest& req,
        c::ConnectResponseWriter& writer)
{
    std::vector<uint8_t> body = req.body;
    if (body.size() >= 5) {
        auto d = c::DecodeFrame(std::span<const uint8_t>(body));
        if (d.bytes_consumed > 0)
            body = { d.payload.begin(), d.payload.end() };
    }

    oscilloscope_interface::v2::SetSettingsRequest update_req;
    if (!update_req.ParseFromArray(body.data(), static_cast<int>(body.size()))) {
        writer.SendHeaders(c::kHttpBadRequest, "application/json");
        writer.WriteError(std::string(c::kCodeInvalidArgument), "parse error");
        return;
    }

    bool restart_needed = false;
    {
        std::lock_guard<std::mutex> lk(settings_mutex_);
        restart_needed = ApplyProto(update_req.settings(), update_req.update_mask());
    }
    SaveSettings();  // persist to device_settings.json

    oscilloscope_interface::v2::SetSettingsResponse resp;
    {
        std::lock_guard<std::mutex> lk(settings_mutex_);
        FillProto(*resp.mutable_settings());
    }
    resp.set_stream_restart_needed(restart_needed);

    std::vector<uint8_t> wire(resp.ByteSizeLong());
    resp.SerializeToArray(wire.data(), static_cast<int>(wire.size()));
    writer.SendHeaders(c::kHttpOk, std::string(c::kContentTypeProto));
    writer.WriteUnary(std::span<const uint8_t>(wire));
}

// ═══════════════════════════════════════════════════════════════════════════════
// StreamData
//
//  Data flow:
//    AdcReader::ReadChunk(frame_size_samples)   — emulates continuous ADC
//      │  loop paced at frame_frequency_hz via sleep
//      ▼
//    Display decimation: only every (frame_freq / display_freq)-th frame forwarded
//      ▼
//    In-frame decimation: keep every decimation_rate-th sample
//      ▼
//    DataChunk over Connect streaming RPC, echoing frame_size + decimation_rate
// ═══════════════════════════════════════════════════════════════════════════════

void OscilloscopeServiceImpl::HandleStreamData(
        const c::ParsedConnectRequest& req,
        c::ConnectResponseWriter& writer)
{
    // Decode Connect frame wrapper
    if (req.body.size() < 5) {
        writer.SendHeaders(c::kHttpBadRequest, "application/json");
        writer.WriteError(std::string(c::kCodeInvalidArgument), "missing frame"); return;
    }
    auto decoded = c::DecodeFrame(std::span<const uint8_t>(req.body));
    if (decoded.bytes_consumed == 0) {
        writer.SendHeaders(c::kHttpBadRequest, "application/json");
        writer.WriteError(std::string(c::kCodeInvalidArgument), "malformed frame"); return;
    }
    oscilloscope_interface::v2::StreamDataRequest stream_req;
    if (!stream_req.ParseFromArray(decoded.payload.data(),
                                   static_cast<int>(decoded.payload.size()))) {
        writer.SendHeaders(c::kHttpBadRequest, "application/json");
        writer.WriteError(std::string(c::kCodeInvalidArgument), "parse error"); return;
    }

    // Validate stream token
    auto claims = stream_token_.Validate(stream_req.stream_token());
    if (!claims) {
        writer.SendHeaders(c::kHttpUnauthorized, "application/json");
        writer.WriteError(std::string(c::kCodeUnauthenticated),
                          "invalid or expired stream token"); return;
    }

    // Snapshot server settings
    OscServiceSettings snap;
    { std::lock_guard<std::mutex> lk(settings_mutex_); snap = settings_; }

    // Resolve parameters — per-connection overrides win over server settings
    const uint32_t frame_size =
            stream_req.frame_size() > 0 ? stream_req.frame_size() : snap.frame_size_samples;
    const uint32_t display_freq_hz =
            stream_req.display_frequency_hz() > 0 ? stream_req.display_frequency_hz()
                                                  : snap.display_frequency_hz;
    const uint32_t decim =
            stream_req.decimation_rate() > 0 ? stream_req.decimation_rate()
                                             : snap.decimation_rate;

    const uint32_t frame_freq_hz = snap.frame_frequency_hz > 0 ? snap.frame_frequency_hz : 1000u;
    const auto     frame_period  = std::chrono::microseconds(1'000'000u / frame_freq_hz);

    // How many produced frames to skip between display frames
    const uint32_t drop_ratio =
            (display_freq_hz > 0 && frame_freq_hz > display_freq_hz)
            ? (frame_freq_hz / display_freq_hz) : 1u;

    // Effective samples-per-channel after in-frame decimation
    const uint32_t eff_samples = (frame_size + decim - 1) / decim;
    const uint32_t ch          = snap.channels;

    spdlog::debug("[StreamData] frame_size={} frame_freq={}Hz display_freq={}Hz "
                  "decim={} drop_ratio={} eff_samples={}",
                  frame_size, frame_freq_hz, display_freq_hz, decim, drop_ratio, eff_samples);

    writer.SendHeaders(c::kHttpOk, std::string(c::kContentTypeConnectProto));

    AdcReader adc(static_cast<uint32_t>(snap.sample_rate_hz), ch);
    uint64_t  seq       = 0;
    uint32_t  frame_idx = 0;

    while (writer.IsClientConnected()) {
        const auto frame_start = std::chrono::steady_clock::now();

        // Acquire one full frame from the emulated continuous ADC stream
        const auto raw      = adc.ReadChunk(frame_size);
        const auto* raw_f32 = reinterpret_cast<const float*>(raw.data());

        // Display decimation — send only every drop_ratio-th frame
        if (frame_idx % drop_ratio == 0) {
            // In-frame decimation — keep every decim-th sample across all channels
            std::vector<float> out_samples;
            out_samples.reserve(eff_samples * ch);
            for (uint32_t i = 0; i < frame_size; i += decim)
                for (uint32_t ci = 0; ci < ch; ++ci)
                    out_samples.push_back(raw_f32[i * ch + ci]);

            oscilloscope_interface::v2::DataChunk chunk;
            chunk.set_samples(
                    reinterpret_cast<const char*>(out_samples.data()),
                    out_samples.size() * sizeof(float));
            chunk.set_sequence_number(seq++);
            chunk.set_timestamp_ns(static_cast<uint64_t>(
                                           std::chrono::duration_cast<std::chrono::nanoseconds>(
                                                   std::chrono::system_clock::now().time_since_epoch()).count()));
            chunk.set_tier(oscilloscope_interface::v2::DECIMATION_TIER_PREVIEW);
            chunk.set_sample_rate_hz(snap.sample_rate_hz);
            chunk.set_channels(ch);
            chunk.set_frame_size(eff_samples);
            chunk.set_decimation_rate(decim);

            std::vector<uint8_t> wire(chunk.ByteSizeLong());
            chunk.SerializeToArray(wire.data(), static_cast<int>(wire.size()));
            writer.WriteStreamingFrame(std::span<const uint8_t>(wire));
        }

        ++frame_idx;

        // Sleep to honour frame_frequency_hz — simulates real hardware pacing
        const auto elapsed = std::chrono::steady_clock::now() - frame_start;
        if (elapsed < frame_period)
            std::this_thread::sleep_for(frame_period - elapsed);
    }
    writer.WriteEndOfStream();
}

// ═══════════════════════════════════════════════════════════════════════════════
// StreamSpectrogram
// ═══════════════════════════════════════════════════════════════════════════════

namespace {

    inline float hann(uint32_t i, uint32_t N) {
        return 0.5f * (1.0f - std::cos(
                2.0f * static_cast<float>(std::numbers::pi) * i / static_cast<float>(N - 1)));
    }

// Naive O(N²) DFT — adequate for fft_size ≤ 1024 in the emulator.
// Returns (N/2 + 1) magnitude values in dBFS.
    std::vector<float> MagnitudeDb(const float* samples, uint32_t N) {
        const uint32_t bins = N / 2 + 1;
        std::vector<float> out(bins);
        for (uint32_t k = 0; k < bins; ++k) {
            double re = 0.0, im = 0.0;
            for (uint32_t n = 0; n < N; ++n) {
                const double angle = -2.0 * std::numbers::pi * k * n / N;
                const double w     = hann(n, N);
                re += w * samples[n] * std::cos(angle);
                im += w * samples[n] * std::sin(angle);
            }
            const double mag = std::sqrt(re * re + im * im) / (N * 0.5);
            out[k] = static_cast<float>(20.0 * std::log10(std::max(mag, 1e-10)));
        }
        return out;
    }

// Power-sum (linear) then back to dB — collapses multiple channel spectra.
    void AccumDb(std::vector<float>& acc, const std::vector<float>& src) {
        for (size_t i = 0; i < acc.size() && i < src.size(); ++i) {
            const float a = std::pow(10.0f, acc[i] / 20.0f);
            const float b = std::pow(10.0f, src[i] / 20.0f);
            acc[i] = 20.0f * std::log10(a + b + 1e-10f);
        }
    }

}  // namespace

void OscilloscopeServiceImpl::HandleStreamSpectrogram(
        const c::ParsedConnectRequest& req,
        c::ConnectResponseWriter& writer)
{
    if (req.body.size() < 5) {
        writer.SendHeaders(c::kHttpBadRequest, "application/json");
        writer.WriteError(std::string(c::kCodeInvalidArgument), "missing frame"); return;
    }
    auto decoded = c::DecodeFrame(std::span<const uint8_t>(req.body));
    if (decoded.bytes_consumed == 0) {
        writer.SendHeaders(c::kHttpBadRequest, "application/json");
        writer.WriteError(std::string(c::kCodeInvalidArgument), "malformed frame"); return;
    }
    oscilloscope_interface::v2::StreamSpectrogramRequest spec_req;
    if (!spec_req.ParseFromArray(decoded.payload.data(),
                                 static_cast<int>(decoded.payload.size()))) {
        writer.SendHeaders(c::kHttpBadRequest, "application/json");
        writer.WriteError(std::string(c::kCodeInvalidArgument), "parse error"); return;
    }

    OscServiceSettings snap;
    { std::lock_guard<std::mutex> lk(settings_mutex_); snap = settings_; }

    const uint32_t fft_size =
            spec_req.fft_size() > 0 ? spec_req.fft_size() : snap.spectrogram_fft_size;
    const uint32_t fps =
            spec_req.target_fps() > 0 ? spec_req.target_fps() : snap.spectrogram_fps;
    const auto frame_period =
            std::chrono::microseconds(fps > 0 ? 1'000'000u / fps : 100'000u);

    spdlog::debug("[StreamSpectrogram] fft_size={} fps={}", fft_size, fps);
    writer.SendHeaders(c::kHttpOk, std::string(c::kContentTypeConnectProto));

    AdcReader adc(static_cast<uint32_t>(snap.sample_rate_hz), snap.channels);
    uint64_t  seq = 0;
    std::vector<float> time_ch(fft_size);

    while (writer.IsClientConnected()) {
        const auto frame_start = std::chrono::steady_clock::now();
        const auto raw         = adc.ReadChunk(fft_size);
        const auto* raw_f32    = reinterpret_cast<const float*>(raw.data());

        // Compute per-channel spectra, power-sum → single-channel output
        std::vector<float> summed;
        for (uint32_t ci = 0; ci < snap.channels; ++ci) {
            for (uint32_t i = 0; i < fft_size; ++i)
                time_ch[i] = raw_f32[i * snap.channels + ci];
            auto spec = MagnitudeDb(time_ch.data(), fft_size);
            if (ci == 0) summed = std::move(spec);
            else         AccumDb(summed, spec);
        }

        oscilloscope_interface::v2::SpectrogramChunk chunk;
        chunk.set_magnitudes_db(
                reinterpret_cast<const char*>(summed.data()),
                summed.size() * sizeof(float));
        chunk.set_sequence_number(seq++);
        chunk.set_timestamp_ns(static_cast<uint64_t>(
                                       std::chrono::duration_cast<std::chrono::nanoseconds>(
                                               std::chrono::system_clock::now().time_since_epoch()).count()));
        chunk.set_fft_size(fft_size);
        chunk.set_freq_resolution_hz(
                static_cast<float>(snap.sample_rate_hz) / static_cast<float>(fft_size));
        chunk.set_sample_rate_hz(snap.sample_rate_hz);
        chunk.set_channels(1);  // single summed output

        std::vector<uint8_t> wire(chunk.ByteSizeLong());
        chunk.SerializeToArray(wire.data(), static_cast<int>(wire.size()));
        writer.WriteStreamingFrame(std::span<const uint8_t>(wire));

        const auto elapsed = std::chrono::steady_clock::now() - frame_start;
        if (elapsed < frame_period)
            std::this_thread::sleep_for(frame_period - elapsed);
    }
    writer.WriteEndOfStream();
}

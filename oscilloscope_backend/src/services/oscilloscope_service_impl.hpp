// FILE: oscilloscope_backend/src/services/oscilloscope_service_impl.hpp
#ifndef OSCILLOSCOPE_BACKEND_SERVICES_OSCILLOSCOPE_SERVICE_IMPL_HPP
#define OSCILLOSCOPE_BACKEND_SERVICES_OSCILLOSCOPE_SERVICE_IMPL_HPP

#include "buf_connect_server/server.hpp"
#include "buf_connect_server/auth/stream_token.hpp"
#include "oscilloscope_interface.pb.h"
#include <google/protobuf/field_mask.pb.h>
#include <mutex>
#include <string>
#include <cstdint>

// ─── In-memory settings bag ───────────────────────────────────────────────────
struct OscServiceSettings {
    // ── Emulation / acquisition pipeline ─────────────────────────────────────
    uint64_t sample_rate_hz       = 2'500'000'000ULL;
    uint32_t frame_size_samples   = 2500;
    uint32_t frame_frequency_hz   = 1000;
    uint32_t display_frequency_hz = 30;
    uint32_t decimation_rate      = 1;
    uint32_t channels             = 2;

    // ── Spectrogram ───────────────────────────────────────────────────────────
    uint32_t spectrogram_fft_size = 512;
    uint32_t spectrogram_fps      = 10;

    // ── Rendering ─────────────────────────────────────────────────────────────
    uint32_t interpolation        = 0;
    bool     persistence_enabled  = false;
    float    persistence_decay    = 0.85f;
    float    grid_opacity         = 0.35f;
    uint32_t display_theme        = 0;
    bool     natural_units        = false;

    // ── Synchronisation / DAQ ─────────────────────────────────────────────────
    uint32_t daq_mode          = 1;   // 1=DAQ_MODE_1, 2=DAQ_MODE_2
    uint32_t pre_delay_samples = 0;

    // ── Spectrogram device settings ───────────────────────────────────────────
    float    spec_setting_one   = 0.0f;
    float    spec_setting_two   = 0.0f;
    float    spec_setting_three = 0.0f;
    uint32_t spec_setting_four  = 0;   // splitter index

    // ── Archive ───────────────────────────────────────────────────────────────
    bool        write_adc                = false;
    std::string adc_archive_path         = "/opt/osc_archive/adc";
    bool        write_spectrogram        = false;
    std::string spectrogram_archive_path = "/opt/osc_archive/spectrogram";

    // ── Trigger ───────────────────────────────────────────────────────────────
    uint32_t trigger_mode    = 1;
    uint32_t trigger_edge    = 1;
    float    trigger_level_v = 0.0f;
    uint32_t trigger_channel = 0;

    // ── Channels (up to 4) ────────────────────────────────────────────────────
    bool  ch_enabled[4]         = { true,  true,  false, false };
    float ch_volts_per_div[4]   = { 1.0f,  1.0f,  1.0f,  1.0f };
    float ch_vertical_offset[4] = { 0.0f,  0.0f,  0.0f,  0.0f };
};

// ─── Service implementation ───────────────────────────────────────────────────
class OscilloscopeServiceImpl : public buf_connect_server::ServiceHandlerBase {
public:
    explicit OscilloscopeServiceImpl(
            std::string jwt_secret    = {},
            std::string settings_path = "device_settings.json");

    std::string ServicePath() const override;
    void RegisterRoutes(buf_connect_server::BufConnectServer& server) override;

private:
    void HandleStreamData(
            const buf_connect_server::connect::ParsedConnectRequest&,
            buf_connect_server::connect::ConnectResponseWriter&);

    void HandleStreamSpectrogram(
            const buf_connect_server::connect::ParsedConnectRequest&,
            buf_connect_server::connect::ConnectResponseWriter&);

    void HandleGetSettings(
            const buf_connect_server::connect::ParsedConnectRequest&,
            buf_connect_server::connect::ConnectResponseWriter&);

    void HandleSetSettings(
            const buf_connect_server::connect::ParsedConnectRequest&,
            buf_connect_server::connect::ConnectResponseWriter&);

    void HandleBrowseDirectory(
            const buf_connect_server::connect::ParsedConnectRequest&,
            buf_connect_server::connect::ConnectResponseWriter&);

    void LoadSettings();
    void SaveSettings() const;

    void FillProto(oscilloscope_interface::v2::OscilloscopeSettings& out) const;
    bool ApplyProto(const oscilloscope_interface::v2::OscilloscopeSettings& s,
                    const google::protobuf::FieldMask& mask);

    buf_connect_server::auth::StreamToken stream_token_;
    std::string                           settings_path_;

    mutable std::mutex settings_mutex_;
    OscServiceSettings settings_;
};

#endif

// FILE: oscilloscope_backend/src/hardware/adc_reader.cpp
#include "hardware/adc_reader.hpp"
#include <cmath>
#include <vector>
#include <chrono>

AdcReader::AdcReader(uint32_t sample_rate_hz, uint32_t channels)
        : sample_rate_hz_(sample_rate_hz)
        , channels_(channels)
        , rng_(static_cast<unsigned>(
                       std::chrono::steady_clock::now().time_since_epoch().count()))
        , noise_dist_(0.0f, 0.04f)   // Gaussian noise, σ = 40 mV
{}

std::string AdcReader::ReadChunk(uint32_t samples_per_channel) {
    const uint32_t total_samples = samples_per_channel * channels_;
    std::vector<float> buf(total_samples);

    for (uint32_t ch = 0; ch < channels_; ++ch) {
        // Each channel gets a distinct frequency: CH0 → 1 kHz, CH1 → 2 kHz, …
        const double freq_hz   = 1000.0 * (ch + 1);
        const float  amplitude = 1.0f;   // ±1 V peak
        const double inv_sr    = 1.0 / sample_rate_hz_;

        for (uint32_t i = 0; i < samples_per_channel; ++i) {
            const double t  = static_cast<double>(sample_index_ + i) * inv_sr;
            const float sig = amplitude *
                              static_cast<float>(std::sin(2.0 * M_PI * freq_hz * t));
            buf[i * channels_ + ch] = sig + noise_dist_(rng_);
        }
    }

    sample_index_ += samples_per_channel;

    // Reinterpret the float buffer as raw bytes
    return std::string(
            reinterpret_cast<const char*>(buf.data()),
            total_samples * sizeof(float));
}

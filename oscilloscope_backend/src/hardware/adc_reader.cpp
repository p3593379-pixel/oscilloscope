// FILE: oscilloscope_backend/src/hardware/adc_reader.cpp
#include "hardware/adc_reader.hpp"
#include <cmath>
#include <cstring>

AdcReader::AdcReader(uint32_t sample_rate_hz, uint32_t channels)
        : sample_rate_hz_(sample_rate_hz), channels_(channels) {}

std::string AdcReader::ReadChunk(uint32_t samples_per_channel) {
    const uint32_t total_samples = samples_per_channel * channels_;
    std::vector<int16_t> buf(total_samples);

    for (uint32_t ch = 0; ch < channels_; ++ch) {
        // Generate sine wave at different frequencies per channel
        double freq_hz = 1000.0 * (ch + 1);
        for (uint32_t i = 0; i < samples_per_channel; ++i) {
            double t = static_cast<double>(sample_index_ + i) / sample_rate_hz_;
            double amplitude = 16384.0;  // ~50% of int16 range
            buf[i * channels_ + ch] =
                    static_cast<int16_t>(amplitude * std::sin(2.0 * M_PI * freq_hz * t));
        }
    }
    sample_index_ += samples_per_channel;

    std::string result(reinterpret_cast<const char*>(buf.data()),
                       total_samples * sizeof(int16_t));
    return result;
}

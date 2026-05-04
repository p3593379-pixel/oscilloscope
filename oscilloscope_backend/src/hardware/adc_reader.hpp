// FILE: oscilloscope_backend/src/hardware/adc_reader.hpp
#ifndef OSCILLOSCOPE_BACKEND_HARDWARE_ADC_READER_HPP
#define OSCILLOSCOPE_BACKEND_HARDWARE_ADC_READER_HPP

#include <cstdint>
#include <random>
#include <string>

// Stub ADC reader: generates noisy multi-channel sinewaves as IEEE-754 float32
// interleaved samples.  Drop-in replacement for real hardware once available.
//
// Wire format: interleaved float32, little-endian.
//   CH0[0], CH1[0], ..., CH0[1], CH1[1], ...
// This matches the Float32Array de-interleave in the web worker.
class AdcReader {
public:
    AdcReader(uint32_t sample_rate_hz, uint32_t channels);

    // Returns `samples_per_channel * channels` interleaved float32 values
    // packed into a std::string (raw bytes).
    std::string ReadChunk(uint32_t samples_per_channel);

private:
    uint32_t sample_rate_hz_;
    uint32_t channels_;
    uint64_t sample_index_ = 0;

    std::default_random_engine            rng_;
    std::normal_distribution<float>       noise_dist_;  // σ = 0.04 V
};

#endif  // OSCILLOSCOPE_BACKEND_HARDWARE_ADC_READER_HPP
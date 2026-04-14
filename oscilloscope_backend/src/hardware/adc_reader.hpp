// FILE: oscilloscope_backend/src/hardware/adc_reader.hpp
#ifndef OSCILLOSCOPE_BACKEND_HARDWARE_ADC_READER_HPP
#define OSCILLOSCOPE_BACKEND_HARDWARE_ADC_READER_HPP

#include <cstdint>
#include <string>
#include <vector>

class AdcReader {
public:
    AdcReader(uint32_t sample_rate_hz, uint32_t channels);
    // Returns raw bytes of interleaved int16_t samples
    std::string ReadChunk(uint32_t samples_per_channel);

private:
    uint32_t sample_rate_hz_;
    uint32_t channels_;
    uint64_t sample_index_ = 0;
};

#endif

// FILE: oscilloscope_backend/src/hardware/dat_file_reader.hpp
#ifndef OSCILLOSCOPE_BACKEND_HARDWARE_DAT_FILE_READER_HPP
#define OSCILLOSCOPE_BACKEND_HARDWARE_DAT_FILE_READER_HPP

#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

class DatFileReader {
public:
    explicit DatFileReader(const std::string& path);
    bool        IsOpen() const;
    uint64_t    FileSize() const;
    std::string ReadAll() const;
    std::optional<std::string> ReadChunk(uint64_t offset, uint64_t length) const;

private:
    std::string path_;
    uint64_t    file_size_ = 0;
    bool        is_open_   = false;
};

#endif

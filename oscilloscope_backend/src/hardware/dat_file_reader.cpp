// FILE: oscilloscope_backend/src/hardware/dat_file_reader.cpp
#include "hardware/dat_file_reader.hpp"
#include <filesystem>
#include <fstream>
#include <stdexcept>

DatFileReader::DatFileReader(const std::string& path) : path_(path) {
    if (std::filesystem::exists(path)) {
        is_open_   = true;
        file_size_ = std::filesystem::file_size(path);
    }
}

bool DatFileReader::IsOpen() const { return is_open_; }
uint64_t DatFileReader::FileSize() const { return file_size_; }

std::string DatFileReader::ReadAll() const {
    if (!is_open_) return {};
    std::ifstream f(path_, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
}

std::optional<std::string> DatFileReader::ReadChunk(
        uint64_t offset, uint64_t length) const {
    if (!is_open_ || offset >= file_size_) return std::nullopt;
    length = std::min(length, file_size_ - offset);
    std::ifstream f(path_, std::ios::binary);
    f.seekg(static_cast<std::streamoff>(offset));
    std::string buf(length, '\0');
    f.read(buf.data(), static_cast<std::streamsize>(length));
    return buf;
}

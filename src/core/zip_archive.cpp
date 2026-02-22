// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "core/zip_archive.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <zlib.h>

namespace dicom_viewer::core {

// ZIP format constants
namespace {

constexpr uint32_t kLocalFileHeaderSignature = 0x04034b50;
constexpr uint32_t kCentralDirHeaderSignature = 0x02014b50;
constexpr uint32_t kEndOfCentralDirSignature = 0x06054b50;
constexpr uint16_t kVersionNeeded = 20;        // 2.0
constexpr uint16_t kVersionMadeBy = 0x031E;    // Unix, version 3.0
constexpr uint16_t kCompressionDeflate = 8;
constexpr uint16_t kCompressionStore = 0;

// Write a little-endian integer to a buffer
template <typename T>
void writeLE(std::vector<uint8_t>& buf, T value) {
    for (size_t i = 0; i < sizeof(T); ++i) {
        buf.push_back(static_cast<uint8_t>(value & 0xFF));
        value >>= 8;
    }
}

// Read a little-endian integer from a buffer at offset
template <typename T>
T readLE(const uint8_t* data, size_t offset) {
    T result = 0;
    for (size_t i = 0; i < sizeof(T); ++i) {
        result |= static_cast<T>(data[offset + i]) << (i * 8);
    }
    return result;
}

// Compress data using zlib deflate
std::expected<std::vector<uint8_t>, ZipError>
deflateData(const std::vector<uint8_t>& input) {
    if (input.empty()) {
        return std::vector<uint8_t>{};
    }

    uLong compBound = compressBound(static_cast<uLong>(input.size()));
    std::vector<uint8_t> output(compBound);

    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(input.data());
    stream.avail_in = static_cast<uInt>(input.size());
    stream.next_out = output.data();
    stream.avail_out = static_cast<uInt>(output.size());

    // Use raw deflate (-MAX_WBITS) for ZIP compatibility
    int ret = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                           -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK) {
        return std::unexpected(ZipError::CompressionFailed);
    }

    ret = deflate(&stream, Z_FINISH);
    deflateEnd(&stream);

    if (ret != Z_STREAM_END) {
        return std::unexpected(ZipError::CompressionFailed);
    }

    output.resize(stream.total_out);
    return output;
}

// Decompress data using zlib inflate
std::expected<std::vector<uint8_t>, ZipError>
inflateData(const std::vector<uint8_t>& input, size_t originalSize) {
    if (input.empty()) {
        return std::vector<uint8_t>(originalSize, 0);
    }

    std::vector<uint8_t> output(originalSize);

    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(input.data());
    stream.avail_in = static_cast<uInt>(input.size());
    stream.next_out = output.data();
    stream.avail_out = static_cast<uInt>(output.size());

    // Use raw inflate (-MAX_WBITS) for ZIP compatibility
    int ret = inflateInit2(&stream, -MAX_WBITS);
    if (ret != Z_OK) {
        return std::unexpected(ZipError::DecompressionFailed);
    }

    ret = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);

    if (ret != Z_STREAM_END) {
        return std::unexpected(ZipError::DecompressionFailed);
    }

    output.resize(stream.total_out);
    return output;
}

} // anonymous namespace

void ZipArchive::addEntry(const std::string& name, const std::vector<uint8_t>& data) {
    entries_[name] = data;
}

void ZipArchive::addEntry(const std::string& name, const std::string& content) {
    entries_[name] = std::vector<uint8_t>(content.begin(), content.end());
}

std::expected<void, ZipError>
ZipArchive::writeTo(const std::filesystem::path& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return std::unexpected(ZipError::FileOpenFailed);
    }

    struct CentralDirEntry {
        std::string name;
        uint32_t crc32;
        uint32_t compressedSize;
        uint32_t uncompressedSize;
        uint32_t localHeaderOffset;
        uint16_t compressionMethod;
    };

    std::vector<CentralDirEntry> centralDir;
    std::vector<uint8_t> buffer;

    // Write local file headers and data
    for (const auto& [name, data] : entries_) {
        CentralDirEntry entry;
        entry.name = name;
        entry.uncompressedSize = static_cast<uint32_t>(data.size());
        entry.crc32 = static_cast<uint32_t>(
            ::crc32(0L, data.data(), static_cast<uInt>(data.size())));
        entry.localHeaderOffset = static_cast<uint32_t>(buffer.size());

        // Compress the data
        std::vector<uint8_t> compressedData;
        uint16_t method;

        if (data.size() > 64) {
            auto compressed = deflateData(data);
            if (!compressed) {
                return std::unexpected(compressed.error());
            }
            compressedData = std::move(*compressed);
            method = kCompressionDeflate;
        } else {
            // Store small entries uncompressed
            compressedData = data;
            method = kCompressionStore;
        }

        entry.compressedSize = static_cast<uint32_t>(compressedData.size());
        entry.compressionMethod = method;

        // Local file header
        writeLE<uint32_t>(buffer, kLocalFileHeaderSignature);
        writeLE<uint16_t>(buffer, kVersionNeeded);
        writeLE<uint16_t>(buffer, 0);  // general purpose bit flag
        writeLE<uint16_t>(buffer, method);
        writeLE<uint16_t>(buffer, 0);  // last mod time
        writeLE<uint16_t>(buffer, 0);  // last mod date
        writeLE<uint32_t>(buffer, entry.crc32);
        writeLE<uint32_t>(buffer, entry.compressedSize);
        writeLE<uint32_t>(buffer, entry.uncompressedSize);
        writeLE<uint16_t>(buffer, static_cast<uint16_t>(name.size()));
        writeLE<uint16_t>(buffer, 0);  // extra field length
        buffer.insert(buffer.end(), name.begin(), name.end());
        buffer.insert(buffer.end(), compressedData.begin(), compressedData.end());

        centralDir.push_back(std::move(entry));
    }

    // Central directory
    uint32_t centralDirOffset = static_cast<uint32_t>(buffer.size());

    for (const auto& entry : centralDir) {
        writeLE<uint32_t>(buffer, kCentralDirHeaderSignature);
        writeLE<uint16_t>(buffer, kVersionMadeBy);
        writeLE<uint16_t>(buffer, kVersionNeeded);
        writeLE<uint16_t>(buffer, 0);  // general purpose bit flag
        writeLE<uint16_t>(buffer, entry.compressionMethod);
        writeLE<uint16_t>(buffer, 0);  // last mod time
        writeLE<uint16_t>(buffer, 0);  // last mod date
        writeLE<uint32_t>(buffer, entry.crc32);
        writeLE<uint32_t>(buffer, entry.compressedSize);
        writeLE<uint32_t>(buffer, entry.uncompressedSize);
        writeLE<uint16_t>(buffer, static_cast<uint16_t>(entry.name.size()));
        writeLE<uint16_t>(buffer, 0);  // extra field length
        writeLE<uint16_t>(buffer, 0);  // file comment length
        writeLE<uint16_t>(buffer, 0);  // disk number start
        writeLE<uint16_t>(buffer, 0);  // internal file attributes
        writeLE<uint32_t>(buffer, 0);  // external file attributes
        writeLE<uint32_t>(buffer, entry.localHeaderOffset);
        buffer.insert(buffer.end(), entry.name.begin(), entry.name.end());
    }

    uint32_t centralDirSize = static_cast<uint32_t>(buffer.size()) - centralDirOffset;

    // End of central directory
    writeLE<uint32_t>(buffer, kEndOfCentralDirSignature);
    writeLE<uint16_t>(buffer, 0);  // disk number
    writeLE<uint16_t>(buffer, 0);  // disk number with central dir
    writeLE<uint16_t>(buffer, static_cast<uint16_t>(centralDir.size()));
    writeLE<uint16_t>(buffer, static_cast<uint16_t>(centralDir.size()));
    writeLE<uint32_t>(buffer, centralDirSize);
    writeLE<uint32_t>(buffer, centralDirOffset);
    writeLE<uint16_t>(buffer, 0);  // comment length

    file.write(reinterpret_cast<const char*>(buffer.data()),
               static_cast<std::streamsize>(buffer.size()));

    if (!file) {
        return std::unexpected(ZipError::FileWriteFailed);
    }

    return {};
}

std::expected<ZipArchive, ZipError>
ZipArchive::readFrom(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return std::unexpected(ZipError::FileOpenFailed);
    }

    auto fileSize = file.tellg();
    file.seekg(0);

    std::vector<uint8_t> data(static_cast<size_t>(fileSize));
    file.read(reinterpret_cast<char*>(data.data()),
              static_cast<std::streamsize>(fileSize));

    if (!file) {
        return std::unexpected(ZipError::FileReadFailed);
    }

    // Minimum ZIP file size: end of central directory = 22 bytes
    if (data.size() < 22) {
        return std::unexpected(ZipError::InvalidArchive);
    }

    // Find end of central directory record (search backwards)
    size_t eocdOffset = 0;
    bool found = false;
    for (size_t i = data.size() - 22; i > 0; --i) {
        if (readLE<uint32_t>(data.data(), i) == kEndOfCentralDirSignature) {
            eocdOffset = i;
            found = true;
            break;
        }
    }

    if (!found) {
        return std::unexpected(ZipError::InvalidArchive);
    }

    uint16_t entryCount = readLE<uint16_t>(data.data(), eocdOffset + 10);
    uint32_t centralDirOffset = readLE<uint32_t>(data.data(), eocdOffset + 16);

    ZipArchive archive;
    size_t offset = centralDirOffset;

    for (uint16_t i = 0; i < entryCount; ++i) {
        if (offset + 46 > data.size()) {
            return std::unexpected(ZipError::InvalidArchive);
        }

        if (readLE<uint32_t>(data.data(), offset) != kCentralDirHeaderSignature) {
            return std::unexpected(ZipError::InvalidArchive);
        }

        uint16_t method = readLE<uint16_t>(data.data(), offset + 10);
        uint32_t crc = readLE<uint32_t>(data.data(), offset + 16);
        uint32_t compSize = readLE<uint32_t>(data.data(), offset + 20);
        uint32_t uncompSize = readLE<uint32_t>(data.data(), offset + 24);
        uint16_t nameLen = readLE<uint16_t>(data.data(), offset + 28);
        uint16_t extraLen = readLE<uint16_t>(data.data(), offset + 30);
        uint16_t commentLen = readLE<uint16_t>(data.data(), offset + 32);
        uint32_t localOffset = readLE<uint32_t>(data.data(), offset + 42);

        std::string name(data.begin() + static_cast<ptrdiff_t>(offset + 46),
                         data.begin() + static_cast<ptrdiff_t>(offset + 46 + nameLen));

        // Read compressed data from local file header
        size_t localDataStart = localOffset + 30;
        uint16_t localNameLen = readLE<uint16_t>(data.data(), localOffset + 26);
        uint16_t localExtraLen = readLE<uint16_t>(data.data(), localOffset + 28);
        localDataStart += localNameLen + localExtraLen;

        if (localDataStart + compSize > data.size()) {
            return std::unexpected(ZipError::InvalidArchive);
        }

        std::vector<uint8_t> compressedData(
            data.begin() + static_cast<ptrdiff_t>(localDataStart),
            data.begin() + static_cast<ptrdiff_t>(localDataStart + compSize));

        // Decompress
        if (method == kCompressionStore) {
            archive.entries_[name] = std::move(compressedData);
        } else if (method == kCompressionDeflate) {
            auto decompressed = inflateData(compressedData, uncompSize);
            if (!decompressed) {
                return std::unexpected(decompressed.error());
            }

            // Verify CRC32
            uint32_t actualCrc = static_cast<uint32_t>(
                ::crc32(0L, decompressed->data(),
                        static_cast<uInt>(decompressed->size())));
            if (actualCrc != crc) {
                return std::unexpected(ZipError::InvalidArchive);
            }

            archive.entries_[name] = std::move(*decompressed);
        } else {
            return std::unexpected(ZipError::InvalidArchive);
        }

        offset += 46 + nameLen + extraLen + commentLen;
    }

    return archive;
}

std::expected<std::vector<uint8_t>, ZipError>
ZipArchive::readEntry(const std::string& name) const {
    auto it = entries_.find(name);
    if (it == entries_.end()) {
        return std::unexpected(ZipError::EntryNotFound);
    }
    return it->second;
}

std::expected<std::string, ZipError>
ZipArchive::readEntryAsString(const std::string& name) const {
    auto result = readEntry(name);
    if (!result) {
        return std::unexpected(result.error());
    }
    return std::string(result->begin(), result->end());
}

bool ZipArchive::hasEntry(const std::string& name) const {
    return entries_.contains(name);
}

std::vector<std::string> ZipArchive::entryNames() const {
    std::vector<std::string> names;
    names.reserve(entries_.size());
    for (const auto& [name, _] : entries_) {
        names.push_back(name);
    }
    return names;
}

} // namespace dicom_viewer::core

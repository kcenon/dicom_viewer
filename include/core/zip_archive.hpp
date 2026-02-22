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

#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace dicom_viewer::core {

/**
 * @brief Error codes for ZIP operations
 */
enum class ZipError {
    FileOpenFailed,
    FileWriteFailed,
    FileReadFailed,
    CompressionFailed,
    DecompressionFailed,
    InvalidArchive,
    EntryNotFound,
    InternalError
};

/**
 * @brief Lightweight ZIP archive reader/writer using system zlib
 *
 * Supports creating ZIP files with multiple text/binary entries and
 * reading entries from existing ZIP files. Uses DEFLATE compression.
 *
 * Usage (write):
 * @code
 * ZipArchive zip;
 * zip.addEntry("manifest.json", manifestJson);
 * zip.addEntry("data/file.bin", binaryData);
 * auto result = zip.writeTo("/path/to/output.zip");
 * @endcode
 *
 * Usage (read):
 * @code
 * auto result = ZipArchive::readFrom("/path/to/archive.zip");
 * if (result) {
 *     auto manifest = result->readEntry("manifest.json");
 * }
 * @endcode
 */
class ZipArchive {
public:
    ZipArchive() = default;

    /**
     * @brief Add an entry to the archive (for writing)
     * @param name Entry name (path within the archive)
     * @param data Entry content
     */
    void addEntry(const std::string& name, const std::vector<uint8_t>& data);

    /**
     * @brief Add a string entry to the archive (for writing)
     * @param name Entry name
     * @param content String content (converted to UTF-8 bytes)
     */
    void addEntry(const std::string& name, const std::string& content);

    /**
     * @brief Write the archive to a file
     * @param path Output file path
     * @return Success or ZipError
     */
    [[nodiscard]] std::expected<void, ZipError>
    writeTo(const std::filesystem::path& path) const;

    /**
     * @brief Read an archive from a file
     * @param path Input file path
     * @return ZipArchive or ZipError
     */
    [[nodiscard]] static std::expected<ZipArchive, ZipError>
    readFrom(const std::filesystem::path& path);

    /**
     * @brief Read a specific entry from the archive
     * @param name Entry name
     * @return Entry data or ZipError
     */
    [[nodiscard]] std::expected<std::vector<uint8_t>, ZipError>
    readEntry(const std::string& name) const;

    /**
     * @brief Read a specific entry as a string
     * @param name Entry name
     * @return Entry content as string or ZipError
     */
    [[nodiscard]] std::expected<std::string, ZipError>
    readEntryAsString(const std::string& name) const;

    /**
     * @brief Check if an entry exists
     */
    [[nodiscard]] bool hasEntry(const std::string& name) const;

    /**
     * @brief Get list of entry names
     */
    [[nodiscard]] std::vector<std::string> entryNames() const;

private:
    std::map<std::string, std::vector<uint8_t>> entries_;
};

} // namespace dicom_viewer::core

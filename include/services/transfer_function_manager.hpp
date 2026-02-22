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

/**
 * @file transfer_function_manager.hpp
 * @brief Transfer function preset management with file I/O
 * @details Manages built-in and custom transfer function presets for volume
 *          rendering. Provides CRUD operations for custom presets,
 *          file-based persistence (save/load/export/import), and
 *          a library of built-in CT/MRI presets.
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "services/volume_renderer.hpp"

namespace dicom_viewer::services {

/// Error types for transfer function operations
enum class TransferFunctionError {
    FileNotFound,
    InvalidFormat,
    ParseError,
    WriteError,
    PresetNotFound,
    DuplicatePreset
};

/// Error result with message
struct TransferFunctionErrorInfo {
    TransferFunctionError code;
    std::string message;
};

/**
 * @brief Manages transfer function presets with save/load functionality
 *
 * Provides preset management including built-in presets, custom presets,
 * and file-based persistence using JSON format.
 *
 * @trace SRS-FR-006
 */
class TransferFunctionManager {
public:
    TransferFunctionManager();
    ~TransferFunctionManager();

    // Non-copyable, movable
    TransferFunctionManager(const TransferFunctionManager&) = delete;
    TransferFunctionManager& operator=(const TransferFunctionManager&) = delete;
    TransferFunctionManager(TransferFunctionManager&&) noexcept;
    TransferFunctionManager& operator=(TransferFunctionManager&&) noexcept;

    /**
     * @brief Get all available preset names
     * @return List of preset names (built-in and custom)
     */
    std::vector<std::string> getPresetNames() const;

    /**
     * @brief Get all built-in preset names
     * @return List of built-in preset names
     */
    std::vector<std::string> getBuiltInPresetNames() const;

    /**
     * @brief Get all custom preset names
     * @return List of custom preset names
     */
    std::vector<std::string> getCustomPresetNames() const;

    /**
     * @brief Get a preset by name
     * @param name Preset name
     * @return Preset if found, error otherwise
     */
    std::expected<TransferFunctionPreset, TransferFunctionErrorInfo>
    getPreset(const std::string& name) const;

    /**
     * @brief Add a custom preset
     * @param preset Preset to add
     * @param overwrite If true, overwrite existing preset with same name
     * @return Success or error
     */
    std::expected<void, TransferFunctionErrorInfo>
    addCustomPreset(const TransferFunctionPreset& preset, bool overwrite = false);

    /**
     * @brief Remove a custom preset
     * @param name Preset name to remove
     * @return Success or error (cannot remove built-in presets)
     */
    std::expected<void, TransferFunctionErrorInfo>
    removeCustomPreset(const std::string& name);

    /**
     * @brief Check if a preset is built-in
     * @param name Preset name
     * @return True if built-in preset
     */
    bool isBuiltInPreset(const std::string& name) const;

    /**
     * @brief Save all custom presets to file
     * @param filePath Path to save file (JSON format)
     * @return Success or error
     */
    std::expected<void, TransferFunctionErrorInfo>
    saveCustomPresets(const std::filesystem::path& filePath) const;

    /**
     * @brief Load custom presets from file
     * @param filePath Path to preset file (JSON format)
     * @param merge If true, merge with existing; if false, replace
     * @return Number of presets loaded or error
     */
    std::expected<size_t, TransferFunctionErrorInfo>
    loadCustomPresets(const std::filesystem::path& filePath, bool merge = true);

    /**
     * @brief Export a single preset to file
     * @param name Preset name
     * @param filePath Path to save file
     * @return Success or error
     */
    std::expected<void, TransferFunctionErrorInfo>
    exportPreset(const std::string& name, const std::filesystem::path& filePath) const;

    /**
     * @brief Import a single preset from file
     * @param filePath Path to preset file
     * @param overwrite If true, overwrite existing preset with same name
     * @return Imported preset name or error
     */
    std::expected<std::string, TransferFunctionErrorInfo>
    importPreset(const std::filesystem::path& filePath, bool overwrite = false);

    /**
     * @brief Create a new preset from color/opacity points
     * @param name Preset name
     * @param windowWidth Window width
     * @param windowCenter Window center
     * @param colorPoints Color transfer function points
     * @param opacityPoints Opacity transfer function points
     * @param gradientOpacityPoints Optional gradient opacity points
     * @return Created preset
     */
    static TransferFunctionPreset createPreset(
        const std::string& name,
        double windowWidth,
        double windowCenter,
        const std::vector<std::tuple<double, double, double, double>>& colorPoints,
        const std::vector<std::pair<double, double>>& opacityPoints,
        const std::vector<std::pair<double, double>>& gradientOpacityPoints = {});

    /**
     * @brief Get the default presets directory
     * @return Path to default presets directory
     */
    static std::filesystem::path getDefaultPresetsDirectory();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

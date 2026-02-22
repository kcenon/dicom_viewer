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

#include "segmentation_label.hpp"
#include "threshold_segmenter.hpp"

#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <itkImage.h>
#include <itkSmartPointer.h>

namespace dicom_viewer::services {

/**
 * @brief Supported file formats for segmentation import/export
 */
enum class SegmentationFormat {
    NIfTI,  ///< NIfTI format (.nii, .nii.gz)
    NRRD    ///< NRRD format (.nrrd)
};

/**
 * @brief Manager for multi-label segmentation
 *
 * Provides comprehensive management of multiple segmentation labels including:
 * - Label creation, modification, and deletion
 * - Active label selection for editing
 * - Label visibility and appearance control
 * - Statistics computation for each label
 * - Import/export of segmentation data
 *
 * Supports up to 255 labels (0 reserved for background).
 *
 * @example
 * @code
 * LabelManager manager;
 * manager.initializeLabelMap(512, 512, 100);
 *
 * // Add labels
 * auto& liver = manager.addLabel("Liver", LabelColor(0.8f, 0.2f, 0.2f));
 * auto& kidney = manager.addLabel("Kidney", LabelColor(0.2f, 0.8f, 0.2f));
 *
 * // Set active label for editing
 * manager.setActiveLabel(liver.id);
 *
 * // Toggle visibility
 * manager.setLabelVisibility(kidney.id, false);
 *
 * // Export segmentation
 * manager.exportSegmentation("/path/to/output.nii.gz", SegmentationFormat::NIfTI);
 * @endcode
 *
 * @trace SRS-FR-024
 */
class LabelManager {
public:
    /// Label map type (3D volume with label IDs)
    using LabelMapType = itk::Image<uint8_t, 3>;

    /// Source image type for statistics computation
    using SourceImageType = itk::Image<short, 3>;

    /// Callback when labels change
    using LabelChangeCallback = std::function<void()>;

    /// Callback when label map is modified
    using LabelMapChangeCallback = std::function<void()>;

    /// Maximum number of labels (excluding background)
    static constexpr uint8_t MAX_LABELS = 255;

    LabelManager();
    ~LabelManager();

    // Non-copyable but movable
    LabelManager(const LabelManager&) = delete;
    LabelManager& operator=(const LabelManager&) = delete;
    LabelManager(LabelManager&&) noexcept;
    LabelManager& operator=(LabelManager&&) noexcept;

    // =========================================================================
    // Label Map Management
    // =========================================================================

    /**
     * @brief Initialize an empty label map with given dimensions
     *
     * @param width Image width in pixels
     * @param height Image height in pixels
     * @param depth Number of slices
     * @param spacing Voxel spacing in mm (default 1.0, 1.0, 1.0)
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, SegmentationError> initializeLabelMap(
        int width,
        int height,
        int depth,
        std::array<double, 3> spacing = {1.0, 1.0, 1.0}
    );

    /**
     * @brief Set an existing label map
     *
     * @param labelMap Label map to use
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, SegmentationError>
    setLabelMap(LabelMapType::Pointer labelMap);

    /**
     * @brief Get the current label map
     * @return Label map pointer or nullptr if not initialized
     */
    [[nodiscard]] LabelMapType::Pointer getLabelMap() const;

    /**
     * @brief Check if label map is initialized
     * @return true if label map exists
     */
    [[nodiscard]] bool hasLabelMap() const noexcept;

    // =========================================================================
    // Label Management
    // =========================================================================

    /**
     * @brief Add a new label with automatic ID assignment
     *
     * @param name Label name
     * @param color Label color (optional, uses palette if not specified)
     * @return Reference to the created label, or error if max labels reached
     */
    [[nodiscard]] std::expected<std::reference_wrapper<SegmentationLabel>, SegmentationError>
    addLabel(const std::string& name, std::optional<LabelColor> color = std::nullopt);

    /**
     * @brief Add a new label with specific ID
     *
     * @param id Label ID (1-255)
     * @param name Label name
     * @param color Label color
     * @return Reference to the created label, or error if ID is invalid or taken
     */
    [[nodiscard]] std::expected<std::reference_wrapper<SegmentationLabel>, SegmentationError>
    addLabel(uint8_t id, const std::string& name, const LabelColor& color);

    /**
     * @brief Remove a label by ID
     *
     * Removes the label and optionally clears its pixels in the label map.
     *
     * @param id Label ID to remove
     * @param clearPixels If true, set all pixels with this label to background
     * @return Success or error if label not found
     */
    [[nodiscard]] std::expected<void, SegmentationError>
    removeLabel(uint8_t id, bool clearPixels = true);

    /**
     * @brief Get a label by ID
     *
     * @param id Label ID
     * @return Pointer to label or nullptr if not found
     */
    [[nodiscard]] SegmentationLabel* getLabel(uint8_t id);

    /**
     * @brief Get a label by ID (const version)
     *
     * @param id Label ID
     * @return Const pointer to label or nullptr if not found
     */
    [[nodiscard]] const SegmentationLabel* getLabel(uint8_t id) const;

    /**
     * @brief Get all labels
     * @return Vector of all labels (sorted by ID)
     */
    [[nodiscard]] std::vector<SegmentationLabel> getAllLabels() const;

    /**
     * @brief Get number of labels (excluding background)
     * @return Label count
     */
    [[nodiscard]] size_t getLabelCount() const noexcept;

    /**
     * @brief Check if a label ID exists
     * @param id Label ID
     * @return true if label exists
     */
    [[nodiscard]] bool hasLabel(uint8_t id) const noexcept;

    /**
     * @brief Clear all labels
     *
     * Removes all labels and optionally clears the label map.
     *
     * @param clearLabelMap If true, set all pixels to background
     */
    void clearAllLabels(bool clearLabelMap = true);

    // =========================================================================
    // Active Label
    // =========================================================================

    /**
     * @brief Set the active label for editing
     *
     * @param id Label ID to make active
     * @return Success or error if label not found
     */
    [[nodiscard]] std::expected<void, SegmentationError> setActiveLabel(uint8_t id);

    /**
     * @brief Get the active label ID
     * @return Active label ID, or 0 if none active
     */
    [[nodiscard]] uint8_t getActiveLabel() const noexcept;

    /**
     * @brief Get the active label object
     * @return Pointer to active label or nullptr
     */
    [[nodiscard]] SegmentationLabel* getActiveLabelObject();

    // =========================================================================
    // Label Properties
    // =========================================================================

    /**
     * @brief Set label name
     *
     * @param id Label ID
     * @param name New name
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, SegmentationError>
    setLabelName(uint8_t id, const std::string& name);

    /**
     * @brief Set label color
     *
     * @param id Label ID
     * @param color New color
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, SegmentationError>
    setLabelColor(uint8_t id, const LabelColor& color);

    /**
     * @brief Set label opacity
     *
     * @param id Label ID
     * @param opacity Opacity value [0.0, 1.0]
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, SegmentationError>
    setLabelOpacity(uint8_t id, double opacity);

    /**
     * @brief Set label visibility
     *
     * @param id Label ID
     * @param visible Visibility state
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, SegmentationError>
    setLabelVisibility(uint8_t id, bool visible);

    /**
     * @brief Toggle label visibility
     *
     * @param id Label ID
     * @return New visibility state, or error
     */
    [[nodiscard]] std::expected<bool, SegmentationError> toggleLabelVisibility(uint8_t id);

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Compute statistics for a specific label
     *
     * @param id Label ID
     * @param sourceImage Source image for HU statistics
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, SegmentationError>
    computeLabelStatistics(uint8_t id, SourceImageType::Pointer sourceImage);

    /**
     * @brief Compute statistics for all labels
     *
     * @param sourceImage Source image for HU statistics
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, SegmentationError>
    computeAllStatistics(SourceImageType::Pointer sourceImage);

    // =========================================================================
    // Import/Export
    // =========================================================================

    /**
     * @brief Export segmentation to file
     *
     * @param path Output file path
     * @param format Output format
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, SegmentationError>
    exportSegmentation(const std::filesystem::path& path, SegmentationFormat format) const;

    /**
     * @brief Import segmentation from file
     *
     * @param path Input file path
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, SegmentationError>
    importSegmentation(const std::filesystem::path& path);

    /**
     * @brief Export label metadata to JSON
     *
     * @param path Output file path
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, SegmentationError>
    exportLabelMetadata(const std::filesystem::path& path) const;

    /**
     * @brief Import label metadata from JSON
     *
     * @param path Input file path
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, SegmentationError>
    importLabelMetadata(const std::filesystem::path& path);

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Set callback for label changes (add/remove/modify)
     * @param callback Callback function
     */
    void setLabelChangeCallback(LabelChangeCallback callback);

    /**
     * @brief Set callback for label map changes
     * @param callback Callback function
     */
    void setLabelMapChangeCallback(LabelMapChangeCallback callback);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace dicom_viewer::services

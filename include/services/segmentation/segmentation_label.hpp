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

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace dicom_viewer::services {

/**
 * @brief RGBA color representation for segmentation labels
 *
 * Stores color components as normalized floating-point values [0.0, 1.0]
 * for compatibility with rendering pipelines.
 */
struct LabelColor {
    float r = 1.0f;  ///< Red component [0.0, 1.0]
    float g = 0.0f;  ///< Green component [0.0, 1.0]
    float b = 0.0f;  ///< Blue component [0.0, 1.0]
    float a = 1.0f;  ///< Alpha component [0.0, 1.0]

    LabelColor() = default;

    /**
     * @brief Construct from RGBA components
     * @param red Red component [0.0, 1.0]
     * @param green Green component [0.0, 1.0]
     * @param blue Blue component [0.0, 1.0]
     * @param alpha Alpha component [0.0, 1.0]
     */
    constexpr LabelColor(float red, float green, float blue, float alpha = 1.0f)
        : r(clamp(red)), g(clamp(green)), b(clamp(blue)), a(clamp(alpha)) {}

    /**
     * @brief Construct from 8-bit RGBA components
     * @param red Red component [0, 255]
     * @param green Green component [0, 255]
     * @param blue Blue component [0, 255]
     * @param alpha Alpha component [0, 255]
     */
    static constexpr LabelColor fromRGBA8(
        uint8_t red,
        uint8_t green,
        uint8_t blue,
        uint8_t alpha = 255
    ) {
        return LabelColor(
            static_cast<float>(red) / 255.0f,
            static_cast<float>(green) / 255.0f,
            static_cast<float>(blue) / 255.0f,
            static_cast<float>(alpha) / 255.0f
        );
    }

    /**
     * @brief Convert to 8-bit RGBA array
     * @return Array of [R, G, B, A] in range [0, 255]
     */
    [[nodiscard]] constexpr std::array<uint8_t, 4> toRGBA8() const {
        return {
            static_cast<uint8_t>(r * 255.0f),
            static_cast<uint8_t>(g * 255.0f),
            static_cast<uint8_t>(b * 255.0f),
            static_cast<uint8_t>(a * 255.0f)
        };
    }

    [[nodiscard]] constexpr bool operator==(const LabelColor& other) const {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }

private:
    static constexpr float clamp(float value) {
        return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
    }
};

/**
 * @brief Segmentation label data structure
 *
 * Represents a single segmentation label with its properties including
 * name, color, visibility, and computed statistics.
 *
 * Labels are identified by a unique ID (1-255), with 0 reserved for background.
 *
 * @example
 * @code
 * SegmentationLabel liver;
 * liver.id = 1;
 * liver.name = "Liver";
 * liver.color = LabelColor(0.8f, 0.2f, 0.2f);
 * liver.opacity = 0.7;
 * liver.visible = true;
 * @endcode
 *
 * @trace SRS-FR-024
 */
struct SegmentationLabel {
    /// Label ID (1-255, 0 is reserved for background)
    uint8_t id = 0;

    /// Human-readable label name (e.g., "Liver", "Kidney")
    std::string name;

    /// Label display color (RGBA)
    LabelColor color;

    /// Opacity for overlay rendering [0.0, 1.0]
    double opacity = 1.0;

    /// Whether the label is visible in views
    bool visible = true;

    /// Computed volume in milliliters (cached, recalculated when mask changes)
    std::optional<double> volumeML;

    /// Mean Hounsfield Unit value within the label region
    std::optional<double> meanHU;

    /// Standard deviation of HU values within the label region
    std::optional<double> stdHU;

    /// Minimum HU value within the label region
    std::optional<double> minHU;

    /// Maximum HU value within the label region
    std::optional<double> maxHU;

    /// Voxel count for this label
    std::optional<uint64_t> voxelCount;

    SegmentationLabel() = default;

    /**
     * @brief Construct a label with basic properties
     * @param labelId Label ID (1-255)
     * @param labelName Label name
     * @param labelColor Label color
     */
    SegmentationLabel(uint8_t labelId, std::string labelName, LabelColor labelColor)
        : id(labelId), name(std::move(labelName)), color(labelColor) {}

    /**
     * @brief Check if this is a valid label (non-background)
     * @return true if label ID is valid (1-255)
     */
    [[nodiscard]] constexpr bool isValid() const noexcept {
        return id > 0;
    }

    /**
     * @brief Clear computed statistics
     *
     * Should be called when the segmentation mask is modified.
     */
    void clearStatistics() {
        volumeML.reset();
        meanHU.reset();
        stdHU.reset();
        minHU.reset();
        maxHU.reset();
        voxelCount.reset();
    }

    [[nodiscard]] bool operator==(const SegmentationLabel& other) const {
        return id == other.id;
    }

    [[nodiscard]] bool operator<(const SegmentationLabel& other) const {
        return id < other.id;
    }
};

/**
 * @brief Predefined color palette for segmentation labels
 *
 * Provides a set of visually distinct colors for up to 20 labels,
 * cycling for additional labels.
 */
class LabelColorPalette {
public:
    /**
     * @brief Get a color for a given label ID
     * @param labelId Label ID (1-255)
     * @return Color for the label
     */
    [[nodiscard]] static LabelColor getColor(uint8_t labelId) {
        if (labelId == 0) {
            return LabelColor(0.0f, 0.0f, 0.0f, 0.0f);  // Background is transparent
        }

        // Predefined palette of 20 distinct colors
        static constexpr std::array<LabelColor, 20> palette = {{
            {0.90f, 0.30f, 0.30f, 1.0f},  // Red
            {0.30f, 0.70f, 0.30f, 1.0f},  // Green
            {0.30f, 0.30f, 0.90f, 1.0f},  // Blue
            {0.90f, 0.90f, 0.30f, 1.0f},  // Yellow
            {0.90f, 0.30f, 0.90f, 1.0f},  // Magenta
            {0.30f, 0.90f, 0.90f, 1.0f},  // Cyan
            {0.90f, 0.60f, 0.30f, 1.0f},  // Orange
            {0.60f, 0.30f, 0.90f, 1.0f},  // Purple
            {0.30f, 0.90f, 0.60f, 1.0f},  // Teal
            {0.90f, 0.30f, 0.60f, 1.0f},  // Pink
            {0.60f, 0.90f, 0.30f, 1.0f},  // Lime
            {0.30f, 0.60f, 0.90f, 1.0f},  // Sky Blue
            {0.70f, 0.50f, 0.30f, 1.0f},  // Brown
            {0.50f, 0.70f, 0.50f, 1.0f},  // Sage
            {0.70f, 0.30f, 0.50f, 1.0f},  // Maroon
            {0.50f, 0.50f, 0.80f, 1.0f},  // Lavender
            {0.80f, 0.80f, 0.50f, 1.0f},  // Khaki
            {0.50f, 0.80f, 0.80f, 1.0f},  // Light Cyan
            {0.80f, 0.50f, 0.80f, 1.0f},  // Orchid
            {0.60f, 0.60f, 0.60f, 1.0f},  // Gray
        }};

        return palette[(labelId - 1) % palette.size()];
    }
};

}  // namespace dicom_viewer::services

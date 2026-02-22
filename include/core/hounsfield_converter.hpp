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
 * @file hounsfield_converter.hpp
 * @brief CT pixel value to Hounsfield Unit conversion and tissue classification
 * @details Provides utilities for converting stored pixel values to Hounsfield
 *          Units using rescale slope and intercept parameters. Includes
 *          tissue type classification, reference HU values for common
 *          tissues, and parameter validation.
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#include <itkImage.h>
#include <itkSmartPointer.h>

namespace dicom_viewer::core {

/**
 * @brief Reference Hounsfield Unit (HU) values for common tissues
 *
 * These values are used for validation and windowing presets.
 * Values are approximate and may vary by scanner and imaging protocol.
 *
 * @trace SRS-FR-004
 */
namespace hounsfield {

/// Standard tissue HU ranges (min, max)
struct TissueRange {
    double min;
    double max;
    const char* name;
};

/// Air HU value
constexpr double Air = -1000.0;

/// Water HU value (by definition)
constexpr double Water = 0.0;

/// Fat tissue range
constexpr TissueRange Fat = {-100.0, -50.0, "Fat"};

/// Lung tissue range
constexpr TissueRange Lung = {-900.0, -500.0, "Lung"};

/// Soft tissue range
constexpr TissueRange SoftTissue = {10.0, 80.0, "Soft Tissue"};

/// Liver tissue range
constexpr TissueRange Liver = {40.0, 60.0, "Liver"};

/// Blood range
constexpr TissueRange Blood = {30.0, 45.0, "Blood"};

/// Muscle range
constexpr TissueRange Muscle = {10.0, 40.0, "Muscle"};

/// Cancellous bone range
constexpr TissueRange CancellousBone = {100.0, 300.0, "Cancellous Bone"};

/// Cortical bone range
constexpr TissueRange CorticalBone = {300.0, 3000.0, "Cortical Bone"};

/// Minimum valid HU value (theoretical)
constexpr double MinHU = -1024.0;

/// Maximum valid HU value for typical CT
constexpr double MaxHU = 3071.0;

/// Default rescale slope
constexpr double DefaultSlope = 1.0;

/// Default rescale intercept
constexpr double DefaultIntercept = 0.0;

/**
 * @brief Check if HU value is within valid range
 * @param hu Hounsfield Unit value
 * @return true if value is valid
 */
constexpr bool isValidHU(double hu) {
    return hu >= MinHU && hu <= MaxHU;
}

/**
 * @brief Check if HU value falls within a tissue range
 * @param hu Hounsfield Unit value
 * @param range Tissue range to check
 * @return true if HU is within range
 */
constexpr bool isInTissueRange(double hu, const TissueRange& range) {
    return hu >= range.min && hu <= range.max;
}

/**
 * @brief Get tissue type name for a given HU value
 * @param hu Hounsfield Unit value
 * @return Tissue name or "Unknown"
 */
const char* getTissueTypeName(double hu);

} // namespace hounsfield

/**
 * @brief Hounsfield Unit converter for CT images
 *
 * Provides utilities for converting CT pixel values to Hounsfield Units
 * using the DICOM rescale slope and intercept formula:
 *   HU = StoredValue × RescaleSlope + RescaleIntercept
 *
 * @trace SRS-FR-004
 */
class HounsfieldConverter {
public:
    using CTImageType = itk::Image<short, 3>;

    /**
     * @brief Rescale parameters extracted from DICOM
     */
    struct RescaleParameters {
        double slope = hounsfield::DefaultSlope;
        double intercept = hounsfield::DefaultIntercept;

        /// Check if parameters are valid (slope must be non-zero)
        [[nodiscard]] bool isValid() const {
            return std::abs(slope) > std::numeric_limits<double>::epsilon();
        }
    };

    /**
     * @brief Convert a single stored value to Hounsfield Units
     * @param storedValue Raw pixel value from DICOM
     * @param slope Rescale slope (0028,1053)
     * @param intercept Rescale intercept (0028,1052)
     * @return Hounsfield Unit value
     */
    static double convert(int storedValue, double slope, double intercept);

    /**
     * @brief Convert a single stored value using RescaleParameters
     * @param storedValue Raw pixel value from DICOM
     * @param params Rescale parameters
     * @return Hounsfield Unit value
     */
    static double convert(int storedValue, const RescaleParameters& params);

    /**
     * @brief Convert HU value back to stored value
     * @param huValue Hounsfield Unit value
     * @param slope Rescale slope
     * @param intercept Rescale intercept
     * @return Stored value
     */
    static int convertToStoredValue(double huValue, double slope, double intercept);

    /**
     * @brief Apply HU conversion to entire 3D image (in-place)
     * @param image ITK CT image to convert
     * @param slope Rescale slope
     * @param intercept Rescale intercept
     * @param clamp Whether to clamp values to valid HU range
     */
    static void applyToImage(
        CTImageType::Pointer image,
        double slope,
        double intercept,
        bool clamp = true
    );

    /**
     * @brief Apply HU conversion to entire 3D image using RescaleParameters
     * @param image ITK CT image to convert
     * @param params Rescale parameters
     * @param clamp Whether to clamp values to valid HU range
     */
    static void applyToImage(
        CTImageType::Pointer image,
        const RescaleParameters& params,
        bool clamp = true
    );

    /**
     * @brief Validate rescale parameters
     * @param slope Rescale slope
     * @param intercept Rescale intercept
     * @return true if parameters are valid
     */
    static bool validateParameters(double slope, double intercept);

    /**
     * @brief Clamp HU value to valid range
     * @param hu Hounsfield Unit value
     * @return Clamped value within [MinHU, MaxHU]
     */
    static double clampHU(double hu);

    /**
     * @brief Get default rescale parameters
     * @return Default RescaleParameters (slope=1.0, intercept=0.0)
     */
    static RescaleParameters getDefaultParameters();
};

} // namespace dicom_viewer::core

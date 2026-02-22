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

#include "threshold_segmenter.hpp"

#include <expected>
#include <functional>
#include <string>

#include <itkImage.h>
#include <itkSmartPointer.h>

namespace dicom_viewer::services {

/**
 * @brief Morphological operation type
 */
enum class MorphologicalOperation {
    Opening,       ///< Remove small protrusions (erosion followed by dilation)
    Closing,       ///< Fill small holes (dilation followed by erosion)
    Dilation,      ///< Expand region boundaries
    Erosion,       ///< Shrink region boundaries
    FillHoles,     ///< Fill internal holes in binary mask
    IslandRemoval  ///< Remove small connected components
};

/**
 * @brief Structuring element shape for morphological operations
 */
enum class StructuringElementShape {
    Ball,   ///< Spherical structuring element (isotropic)
    Cross   ///< Cross-shaped structuring element (faster, anisotropic)
};

/**
 * @brief Morphological post-processing for segmentation refinement
 *
 * Provides morphological operations to refine binary segmentation masks
 * using ITK filters. These operations help clean up segmentation results
 * by removing noise, filling holes, and smoothing boundaries.
 *
 * Supported operations:
 * - Opening: Remove small protrusions (erosion + dilation)
 * - Closing: Fill small holes (dilation + erosion)
 * - Dilation: Expand region boundaries
 * - Erosion: Shrink region boundaries
 * - Fill Holes: Fill internal holes completely
 * - Island Removal: Keep only largest connected components
 *
 * @example
 * @code
 * MorphologicalProcessor processor;
 *
 * // Remove small noise with opening
 * MorphologicalProcessor::Parameters params;
 * params.radius = 2;
 * params.structuringElement = StructuringElementShape::Ball;
 *
 * auto result = processor.apply(binaryMask, MorphologicalOperation::Opening, params);
 * if (result) {
 *     auto cleanedMask = result.value();
 * }
 *
 * // Keep only the largest connected component
 * auto largestResult = processor.keepLargestComponents(binaryMask, 1);
 * @endcode
 *
 * @trace SRS-FR-025
 */
class MorphologicalProcessor {
public:
    /// Binary mask type (input and output)
    using BinaryMaskType = itk::Image<unsigned char, 3>;

    /// Label map type for multi-label operations
    using LabelMapType = itk::Image<unsigned char, 3>;

    /// Progress callback (0.0 to 1.0)
    using ProgressCallback = std::function<void(double progress)>;

    /**
     * @brief Parameters for morphological operations
     */
    struct Parameters {
        /// Structuring element radius in voxels (1-10)
        int radius = 1;

        /// Structuring element shape
        StructuringElementShape structuringElement = StructuringElementShape::Ball;

        /// Foreground value in binary mask
        unsigned char foregroundValue = 1;

        /// Background value in binary mask
        unsigned char backgroundValue = 0;

        /**
         * @brief Validate parameters
         * @return true if parameters are valid
         */
        [[nodiscard]] bool isValid() const noexcept {
            return radius >= 1 && radius <= 10;
        }
    };

    /**
     * @brief Parameters for island removal operation
     */
    struct IslandRemovalParameters {
        /// Number of largest components to keep (1-255)
        int numberOfComponents = 1;

        /// Foreground value in binary mask
        unsigned char foregroundValue = 1;

        /// Attribute to use for sorting (volume by default)
        bool sortByVolume = true;

        /**
         * @brief Validate parameters
         * @return true if parameters are valid
         */
        [[nodiscard]] bool isValid() const noexcept {
            return numberOfComponents >= 1 && numberOfComponents <= 255;
        }
    };

    MorphologicalProcessor() = default;
    ~MorphologicalProcessor() = default;

    // Copyable and movable
    MorphologicalProcessor(const MorphologicalProcessor&) = default;
    MorphologicalProcessor& operator=(const MorphologicalProcessor&) = default;
    MorphologicalProcessor(MorphologicalProcessor&&) noexcept = default;
    MorphologicalProcessor& operator=(MorphologicalProcessor&&) noexcept = default;

    /**
     * @brief Apply morphological operation to binary mask
     *
     * @param input Input binary mask
     * @param operation Morphological operation to apply
     * @param params Operation parameters
     * @return Processed binary mask on success, error on failure
     */
    [[nodiscard]] std::expected<BinaryMaskType::Pointer, SegmentationError>
    apply(
        BinaryMaskType::Pointer input,
        MorphologicalOperation operation,
        const Parameters& params
    ) const;

    /**
     * @brief Apply morphological operation with default parameters
     *
     * @param input Input binary mask
     * @param operation Morphological operation to apply
     * @param radius Structuring element radius (default 1)
     * @return Processed binary mask on success, error on failure
     */
    [[nodiscard]] std::expected<BinaryMaskType::Pointer, SegmentationError>
    apply(
        BinaryMaskType::Pointer input,
        MorphologicalOperation operation,
        int radius = 1
    ) const;

    /**
     * @brief Apply opening operation (erosion followed by dilation)
     *
     * Removes small bright spots and thin protrusions while preserving
     * the overall shape and size of larger objects.
     *
     * @param input Input binary mask
     * @param params Operation parameters
     * @return Processed binary mask on success, error on failure
     */
    [[nodiscard]] std::expected<BinaryMaskType::Pointer, SegmentationError>
    opening(
        BinaryMaskType::Pointer input,
        const Parameters& params
    ) const;

    /**
     * @brief Apply closing operation (dilation followed by erosion)
     *
     * Fills small holes and narrow gaps while preserving
     * the overall shape and size of objects.
     *
     * @param input Input binary mask
     * @param params Operation parameters
     * @return Processed binary mask on success, error on failure
     */
    [[nodiscard]] std::expected<BinaryMaskType::Pointer, SegmentationError>
    closing(
        BinaryMaskType::Pointer input,
        const Parameters& params
    ) const;

    /**
     * @brief Apply dilation operation
     *
     * Expands the foreground region by the structuring element radius.
     *
     * @param input Input binary mask
     * @param params Operation parameters
     * @return Processed binary mask on success, error on failure
     */
    [[nodiscard]] std::expected<BinaryMaskType::Pointer, SegmentationError>
    dilation(
        BinaryMaskType::Pointer input,
        const Parameters& params
    ) const;

    /**
     * @brief Apply erosion operation
     *
     * Shrinks the foreground region by the structuring element radius.
     *
     * @param input Input binary mask
     * @param params Operation parameters
     * @return Processed binary mask on success, error on failure
     */
    [[nodiscard]] std::expected<BinaryMaskType::Pointer, SegmentationError>
    erosion(
        BinaryMaskType::Pointer input,
        const Parameters& params
    ) const;

    /**
     * @brief Fill all internal holes in binary mask
     *
     * Fills any background region completely surrounded by foreground.
     * Unlike closing, this fills holes of any size.
     *
     * @param input Input binary mask
     * @param foregroundValue Foreground value (default 1)
     * @return Processed binary mask on success, error on failure
     */
    [[nodiscard]] std::expected<BinaryMaskType::Pointer, SegmentationError>
    fillHoles(
        BinaryMaskType::Pointer input,
        unsigned char foregroundValue = 1
    ) const;

    /**
     * @brief Keep only the N largest connected components
     *
     * Removes small isolated regions by keeping only the specified number
     * of largest connected components based on volume.
     *
     * @param input Input binary mask
     * @param numComponents Number of components to keep (default 1)
     * @return Processed binary mask on success, error on failure
     */
    [[nodiscard]] std::expected<BinaryMaskType::Pointer, SegmentationError>
    keepLargestComponents(
        BinaryMaskType::Pointer input,
        int numComponents = 1
    ) const;

    /**
     * @brief Keep only the N largest connected components with detailed parameters
     *
     * @param input Input binary mask
     * @param params Island removal parameters
     * @return Processed binary mask on success, error on failure
     */
    [[nodiscard]] std::expected<BinaryMaskType::Pointer, SegmentationError>
    keepLargestComponents(
        BinaryMaskType::Pointer input,
        const IslandRemovalParameters& params
    ) const;

    /**
     * @brief Apply morphological operation to a single 2D slice (for preview)
     *
     * @param input Input 3D binary mask
     * @param sliceIndex Slice index (along Z axis)
     * @param operation Morphological operation to apply
     * @param params Operation parameters
     * @return 2D processed slice on success, error on failure
     */
    [[nodiscard]] std::expected<itk::Image<unsigned char, 2>::Pointer, SegmentationError>
    applyToSlice(
        BinaryMaskType::Pointer input,
        unsigned int sliceIndex,
        MorphologicalOperation operation,
        const Parameters& params
    ) const;

    /**
     * @brief Apply morphological operation to a specific label in label map
     *
     * Extracts the specified label, applies the operation, and merges back.
     *
     * @param labelMap Multi-label segmentation map
     * @param labelId Label ID to process
     * @param operation Morphological operation to apply
     * @param params Operation parameters
     * @return Updated label map on success, error on failure
     */
    [[nodiscard]] std::expected<LabelMapType::Pointer, SegmentationError>
    applyToLabel(
        LabelMapType::Pointer labelMap,
        unsigned char labelId,
        MorphologicalOperation operation,
        const Parameters& params
    ) const;

    /**
     * @brief Apply morphological operation to all labels in label map
     *
     * Applies the operation to each label independently, preserving label IDs.
     *
     * @param labelMap Multi-label segmentation map
     * @param operation Morphological operation to apply
     * @param params Operation parameters
     * @return Updated label map on success, error on failure
     */
    [[nodiscard]] std::expected<LabelMapType::Pointer, SegmentationError>
    applyToAllLabels(
        LabelMapType::Pointer labelMap,
        MorphologicalOperation operation,
        const Parameters& params
    ) const;

    /**
     * @brief Set progress callback for long operations
     * @param callback Progress callback function
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Get string representation of operation type
     * @param operation Operation type
     * @return Operation name as string
     */
    [[nodiscard]] static std::string operationToString(MorphologicalOperation operation);

    /**
     * @brief Get string representation of structuring element shape
     * @param shape Structuring element shape
     * @return Shape name as string
     */
    [[nodiscard]] static std::string structuringElementToString(StructuringElementShape shape);

private:
    ProgressCallback progressCallback_;
};

}  // namespace dicom_viewer::services

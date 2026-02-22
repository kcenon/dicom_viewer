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
 * @file data_serializer.hpp
 * @brief Serialization of image data and analysis results into project files
 * @details Handles reading and writing of volumetric image data, segmentation
 *          masks, and analysis results within .flo project containers.
 *          Uses NRRD format for image serialization and ZIP-based
 *          archiving through ProjectManager.
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "core/project_manager.hpp"
#include "core/zip_archive.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

#include <itkImage.h>
#include <itkVectorImage.h>

#include <nlohmann/json_fwd.hpp>

namespace dicom_viewer::core {

/**
 * @brief Serializer for image data and analysis results into .flo project files
 *
 * Provides NRRD-based serialization for ITK images (scalar, vector, label map)
 * and JSON-based serialization for analysis results. Designed to work with
 * ZipArchive for the .flo project container format.
 *
 * ZIP entry layout:
 * @code
 * data/
 * ├── velocity/
 * │   ├── phase_0000.nrrd       // VectorImage3D (3-component velocity)
 * │   ├── phase_0001.nrrd
 * │   └── ...
 * ├── magnitude/
 * │   ├── phase_0000.nrrd       // FloatImage3D
 * │   └── ...
 * ├── mask/
 * │   ├── label_map.nrrd        // uint8 label map
 * │   └── labels.json           // label definitions (name, color, opacity)
 * └── analysis/
 *     └── results.json          // measurements, flow, hemodynamics
 * @endcode
 *
 * NRRD uses raw encoding (ZIP handles compression via DEFLATE).
 *
 * @trace SRS-FR-050
 */
class DataSerializer {
public:
    using FloatImage3D = itk::Image<float, 3>;
    using VectorImage3D = itk::VectorImage<float, 3>;
    using LabelMapType = itk::Image<uint8_t, 3>;

    /**
     * @brief Definition of a segmentation label
     */
    struct LabelDefinition {
        uint8_t id = 0;
        std::string name;
        std::array<float, 3> color = {1.0f, 0.0f, 0.0f};  ///< RGB [0,1]
        float opacity = 1.0f;
    };

    // =========================================================================
    // Low-level NRRD encoding/decoding (public for testing)
    // =========================================================================

    /**
     * @brief Encode a scalar float image as NRRD bytes (raw encoding)
     * @param image Source image (non-null)
     * @return NRRD header + raw float data
     */
    [[nodiscard]] static std::vector<uint8_t>
    scalarImageToNRRD(const FloatImage3D* image);

    /**
     * @brief Decode NRRD bytes to a scalar float image
     * @param data NRRD bytes (header + raw data)
     * @return Decoded image or ProjectError
     */
    [[nodiscard]] static std::expected<FloatImage3D::Pointer, ProjectError>
    nrrdToScalarImage(const std::vector<uint8_t>& data);

    /**
     * @brief Encode a 3-component vector image as NRRD bytes
     * @param image Source vector image (non-null, 3 components)
     * @return NRRD header + raw float data
     */
    [[nodiscard]] static std::vector<uint8_t>
    vectorImageToNRRD(const VectorImage3D* image);

    /**
     * @brief Decode NRRD bytes to a 3-component vector image
     * @param data NRRD bytes (header + raw data)
     * @return Decoded image or ProjectError
     */
    [[nodiscard]] static std::expected<VectorImage3D::Pointer, ProjectError>
    nrrdToVectorImage(const std::vector<uint8_t>& data);

    /**
     * @brief Encode a uint8 label map as NRRD bytes
     * @param image Source label map (non-null)
     * @return NRRD header + raw uint8 data
     */
    [[nodiscard]] static std::vector<uint8_t>
    labelMapToNRRD(const LabelMapType* image);

    /**
     * @brief Decode NRRD bytes to a uint8 label map
     * @param data NRRD bytes (header + raw data)
     * @return Decoded label map or ProjectError
     */
    [[nodiscard]] static std::expected<LabelMapType::Pointer, ProjectError>
    nrrdToLabelMap(const std::vector<uint8_t>& data);

    // =========================================================================
    // High-level ZIP serialization
    // =========================================================================

    /**
     * @brief Save velocity fields for all phases into a ZipArchive
     *
     * Each phase is stored as data/velocity/phase_NNNN.nrrd.
     *
     * @param zip Archive to add entries to
     * @param velocityPhases Vector of velocity fields (VectorImage3D)
     * @param magnitudePhases Vector of magnitude images (FloatImage3D)
     */
    [[nodiscard]] static std::expected<void, ProjectError>
    saveVelocityData(ZipArchive& zip,
                     const std::vector<VectorImage3D::Pointer>& velocityPhases,
                     const std::vector<FloatImage3D::Pointer>& magnitudePhases);

    /**
     * @brief Load velocity fields from a ZipArchive
     *
     * @param zip Archive to read from
     * @param[out] velocityPhases Loaded velocity fields
     * @param[out] magnitudePhases Loaded magnitude images
     */
    [[nodiscard]] static std::expected<void, ProjectError>
    loadVelocityData(const ZipArchive& zip,
                     std::vector<VectorImage3D::Pointer>& velocityPhases,
                     std::vector<FloatImage3D::Pointer>& magnitudePhases);

    /**
     * @brief Save segmentation mask and label definitions
     *
     * @param zip Archive to add entries to
     * @param labelMap Label map image
     * @param labels Label definitions (name, color, opacity per label)
     */
    [[nodiscard]] static std::expected<void, ProjectError>
    saveMask(ZipArchive& zip,
             const LabelMapType* labelMap,
             const std::vector<LabelDefinition>& labels);

    /**
     * @brief Load segmentation mask and label definitions
     *
     * @param zip Archive to read from
     * @param[out] labelMap Loaded label map
     * @param[out] labels Loaded label definitions
     */
    [[nodiscard]] static std::expected<void, ProjectError>
    loadMask(const ZipArchive& zip,
             LabelMapType::Pointer& labelMap,
             std::vector<LabelDefinition>& labels);

    /**
     * @brief Save analysis results as JSON
     *
     * @param zip Archive to add entries to
     * @param results JSON object with flow metrics, hemodynamics, measurements
     */
    [[nodiscard]] static std::expected<void, ProjectError>
    saveAnalysisResults(ZipArchive& zip, const nlohmann::json& results);

    /**
     * @brief Load analysis results from JSON
     *
     * @param zip Archive to read from
     * @return JSON object or ProjectError
     */
    [[nodiscard]] static std::expected<nlohmann::json, ProjectError>
    loadAnalysisResults(const ZipArchive& zip);
};

}  // namespace dicom_viewer::core

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

#include "services/export/data_exporter.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <itkImage.h>
#include <itkVectorImage.h>

namespace dicom_viewer::services {

/**
 * @brief MAT-file Level 5 writer and velocity field exporter
 *
 * Writes MATLAB .mat files in Level 5 binary format without external
 * library dependencies. Supports float32/float64 numeric arrays (up to 4D)
 * and struct data elements with string fields.
 *
 * Data is stored in column-major (Fortran) order as required by MATLAB.
 *
 * File naming follows the Heartflow convention:
 *   4DPC_vel_AP.mat, 4DPC_vel_FH.mat, 4DPC_vel_RL.mat,
 *   4DPC_M_FFE.mat (magnitude)
 *
 * @trace SRS-FR-050
 */
class MatlabExporter {
public:
    using FloatImage3D = itk::Image<float, 3>;
    using VectorImage3D = itk::VectorImage<float, 3>;

    /**
     * @brief Configuration for velocity field export
     */
    struct ExportConfig {
        std::filesystem::path outputDir;
        std::string prefix = "4DPC";
        int vencValue = 150;
        bool exportMagnitude = true;
    };

    /**
     * @brief DICOM metadata to embed in MAT files
     */
    struct DicomMeta {
        std::string seriesDescription;
        std::string sequenceName;
        std::string imageType;
        double pixelSpacingX = 1.0;
        double pixelSpacingY = 1.0;
        double sliceThickness = 1.0;
    };

    // =====================================================================
    // High-level velocity export
    // =====================================================================

    /**
     * @brief Export multi-phase velocity fields to MAT files
     *
     * Generates separate files for AP, FH, RL components and optionally
     * magnitude. Each file contains a 4D array (x, y, z, t) and metadata.
     *
     * @param velocityPhases Per-phase 3-component velocity (VectorImage3D)
     * @param magnitudePhases Per-phase magnitude (FloatImage3D), optional
     * @param meta DICOM metadata to embed
     * @param config Export configuration
     * @return Success or ExportError
     */
    [[nodiscard]] static std::expected<void, ExportError>
    exportVelocityFields(
        const std::vector<VectorImage3D::Pointer>& velocityPhases,
        const std::vector<FloatImage3D::Pointer>& magnitudePhases,
        const DicomMeta& meta,
        const ExportConfig& config);

    // =====================================================================
    // Low-level MAT-file v5 format writer (public for testing)
    // =====================================================================

    /**
     * @brief Write MAT-file v5 header (128 bytes)
     *
     * Layout: 116 bytes descriptive text + 8 bytes subsys offset +
     *         2 bytes version (0x0100) + 2 bytes endian ('IM')
     *
     * @param out Output buffer to append to
     * @param description Text description (max 116 chars)
     */
    static void writeHeader(std::vector<uint8_t>& out,
                            const std::string& description);

    /**
     * @brief Write a miMATRIX data element containing a float array
     *
     * @param out Output buffer to append to
     * @param name Variable name in MATLAB workspace
     * @param data Float data in column-major order
     * @param dimensions Array dimensions (e.g., {nx, ny, nz, nt})
     */
    static void writeFloatArray(std::vector<uint8_t>& out,
                                const std::string& name,
                                const std::vector<float>& data,
                                const std::vector<int32_t>& dimensions);

    /**
     * @brief Write a miMATRIX data element containing a MATLAB struct
     *
     * @param out Output buffer to append to
     * @param name Variable name
     * @param fields Map of field name → string value
     */
    static void writeStruct(std::vector<uint8_t>& out,
                            const std::string& name,
                            const std::map<std::string, std::string>& fields);

    /**
     * @brief Convert a 3D ITK float image to column-major flat array
     *
     * MATLAB uses column-major (Fortran) order: x varies fastest,
     * then y, then z. ITK stores in row-major (C) order.
     *
     * @param image Input ITK image
     * @return Flat array in column-major order
     */
    [[nodiscard]] static std::vector<float>
    itkToColumnMajor(const FloatImage3D* image);

    /**
     * @brief Extract a single component from a VectorImage3D
     *
     * @param image Input vector image (3 components)
     * @param component Component index (0, 1, or 2)
     * @return Flat array of the selected component in column-major order
     */
    [[nodiscard]] static std::vector<float>
    extractComponentColumnMajor(const VectorImage3D* image, int component);
};

}  // namespace dicom_viewer::services

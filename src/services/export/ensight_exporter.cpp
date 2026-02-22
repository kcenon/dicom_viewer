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

#include "services/export/ensight_exporter.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace dicom_viewer::services {

void EnsightExporter::setProgressCallback(ProgressCallback callback) {
    progressCallback_ = std::move(callback);
}

std::expected<void, ExportError>
EnsightExporter::exportData(const std::vector<PhaseData>& phases,
                            const ExportConfig& config) const {
    if (phases.empty()) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData, "No phase data provided"});
    }

    if (!std::filesystem::exists(config.outputDir)) {
        return std::unexpected(ExportError{
            ExportError::Code::FileAccessDenied,
            "Output directory does not exist: " + config.outputDir.string()});
    }

    // Collect variable names from first phase
    std::vector<std::string> scalarNames;
    std::vector<std::string> vectorNames;
    for (const auto& s : phases[0].scalars) {
        scalarNames.push_back(s.name);
    }
    for (const auto& v : phases[0].vectors) {
        vectorNames.push_back(v.name);
    }

    if (scalarNames.empty() && vectorNames.empty()) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData, "No variables to export"});
    }

    // Collect time values
    std::vector<double> timeValues;
    timeValues.reserve(phases.size());
    for (const auto& p : phases) {
        timeValues.push_back(p.timeValue);
    }

    int numSteps = static_cast<int>(phases.size());
    int totalFiles = numSteps * (static_cast<int>(scalarNames.size())
                                 + static_cast<int>(vectorNames.size()))
                     + 2;  // +2 for case + geo
    int filesWritten = 0;

    auto reportProgress = [&](const std::string& status) {
        if (progressCallback_) {
            double progress = static_cast<double>(filesWritten) / totalFiles;
            progressCallback_(progress, status);
        }
    };

    // 1. Write geometry file (from first available image)
    reportProgress("Writing geometry");
    auto geoPath = config.outputDir / (config.caseName + ".geo");

    const FloatImage3D* refImage = nullptr;
    if (!phases[0].scalars.empty() && phases[0].scalars[0].image) {
        refImage = phases[0].scalars[0].image.GetPointer();
    } else if (!phases[0].vectors.empty() && phases[0].vectors[0].image) {
        // Use VectorImage dimensions by creating a temp reference
        // For geometry, we only need dimensions/spacing/origin, so
        // the first scalar from any phase works too.
        // If only vectors exist, we need a different approach.
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "At least one scalar field is required for geometry reference"});
    }

    if (!refImage) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData, "No image data available"});
    }

    auto geoResult = writeGeometry(geoPath, refImage);
    if (!geoResult) return geoResult;
    ++filesWritten;

    // 2. Write variable files for each phase
    for (int step = 0; step < numSteps; ++step) {
        const auto& phase = phases[step];

        // Write scalar variables
        for (size_t si = 0; si < scalarNames.size(); ++si) {
            std::ostringstream filename;
            filename << config.caseName << "."
                     << scalarNames[si]
                     << std::setw(4) << std::setfill('0') << (step + 1);
            auto varPath = config.outputDir / filename.str();

            reportProgress("Writing " + scalarNames[si]
                           + " phase " + std::to_string(step + 1));

            if (si >= phase.scalars.size() || !phase.scalars[si].image) {
                return std::unexpected(ExportError{
                    ExportError::Code::InvalidData,
                    "Missing scalar '" + scalarNames[si]
                    + "' in phase " + std::to_string(step)});
            }

            auto result = writeScalarVariable(
                varPath, scalarNames[si], phase.scalars[si].image.GetPointer());
            if (!result) return result;
            ++filesWritten;
        }

        // Write vector variables
        for (size_t vi = 0; vi < vectorNames.size(); ++vi) {
            std::ostringstream filename;
            filename << config.caseName << "."
                     << vectorNames[vi]
                     << std::setw(4) << std::setfill('0') << (step + 1);
            auto varPath = config.outputDir / filename.str();

            reportProgress("Writing " + vectorNames[vi]
                           + " phase " + std::to_string(step + 1));

            if (vi >= phase.vectors.size() || !phase.vectors[vi].image) {
                return std::unexpected(ExportError{
                    ExportError::Code::InvalidData,
                    "Missing vector '" + vectorNames[vi]
                    + "' in phase " + std::to_string(step)});
            }

            auto result = writeVectorVariable(
                varPath, vectorNames[vi], phase.vectors[vi].image.GetPointer());
            if (!result) return result;
            ++filesWritten;
        }
    }

    // 3. Write case file last (references all other files)
    reportProgress("Writing case file");
    auto casePath = config.outputDir / (config.caseName + ".case");
    auto caseResult = writeCaseFile(
        casePath, config.caseName, scalarNames, vectorNames,
        numSteps, timeValues);
    if (!caseResult) return caseResult;
    ++filesWritten;

    if (progressCallback_) {
        progressCallback_(1.0, "Export complete");
    }

    return {};
}

// =============================================================================
// Case file writer (ASCII)
// =============================================================================

std::expected<void, ExportError>
EnsightExporter::writeCaseFile(const std::filesystem::path& path,
                               const std::string& caseName,
                               const std::vector<std::string>& scalarNames,
                               const std::vector<std::string>& vectorNames,
                               int numTimeSteps,
                               const std::vector<double>& timeValues) {
    std::ofstream out(path);
    if (!out.is_open()) {
        return std::unexpected(ExportError{
            ExportError::Code::FileAccessDenied,
            "Cannot create case file: " + path.string()});
    }

    // FORMAT section
    out << "FORMAT\n";
    out << "type: ensight gold\n\n";

    // GEOMETRY section
    out << "GEOMETRY\n";
    out << "model: " << caseName << ".geo\n\n";

    // VARIABLE section
    out << "VARIABLE\n";
    for (const auto& name : scalarNames) {
        out << "scalar per node: " << name << " " << caseName
            << "." << name << "****\n";
    }
    for (const auto& name : vectorNames) {
        out << "vector per node: " << name << " " << caseName
            << "." << name << "****\n";
    }
    out << "\n";

    // TIME section
    if (numTimeSteps > 1) {
        out << "TIME\n";
        out << "time set:              1\n";
        out << "number of steps:       " << numTimeSteps << "\n";
        out << "filename start number: 1\n";
        out << "filename increment:    1\n";
        out << "time values:";
        for (int i = 0; i < numTimeSteps; ++i) {
            double tv = (i < static_cast<int>(timeValues.size()))
                        ? timeValues[i] : 0.0;
            out << " " << std::fixed << std::setprecision(6) << tv;
        }
        out << "\n";
    }

    return {};
}

// =============================================================================
// Geometry file writer (C Binary, structured grid)
// =============================================================================

std::expected<void, ExportError>
EnsightExporter::writeGeometry(const std::filesystem::path& path,
                               const FloatImage3D* referenceImage) {
    if (!referenceImage) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData, "Null reference image"});
    }

    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        return std::unexpected(ExportError{
            ExportError::Code::FileAccessDenied,
            "Cannot create geometry file: " + path.string()});
    }

    auto dims = getImageDimensions(referenceImage);
    auto spacing = referenceImage->GetSpacing();
    auto origin = referenceImage->GetOrigin();
    size_t numNodes = static_cast<size_t>(dims[0])
                      * static_cast<size_t>(dims[1])
                      * static_cast<size_t>(dims[2]);

    // Header
    writeBinaryString(out, "C Binary");
    writeBinaryString(out, "Ensight Gold geometry file");
    writeBinaryString(out, "Generated by dicom_viewer");
    writeBinaryString(out, "node id off");
    writeBinaryString(out, "element id off");

    // Part 1
    writeBinaryString(out, "part");
    writeBinaryInt(out, 1);
    writeBinaryString(out, "Structured grid");
    writeBinaryString(out, "block");
    writeBinaryInt(out, dims[0]);
    writeBinaryInt(out, dims[1]);
    writeBinaryInt(out, dims[2]);

    // Write X coordinates for all nodes
    // Ensight block format: all X, then all Y, then all Z
    // Node ordering: i varies fastest, then j, then k
    for (int coord = 0; coord < 3; ++coord) {
        for (int k = 0; k < dims[2]; ++k) {
            for (int j = 0; j < dims[1]; ++j) {
                for (int i = 0; i < dims[0]; ++i) {
                    float value = static_cast<float>(
                        origin[coord] + (coord == 0 ? i : (coord == 1 ? j : k))
                        * spacing[coord]);
                    writeBinaryFloat(out, value);
                }
            }
        }
    }

    return {};
}

// =============================================================================
// Scalar variable writer (C Binary, per node)
// =============================================================================

std::expected<void, ExportError>
EnsightExporter::writeScalarVariable(const std::filesystem::path& path,
                                     const std::string& description,
                                     const FloatImage3D* image) {
    if (!image) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "Null image for variable: " + description});
    }

    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        return std::unexpected(ExportError{
            ExportError::Code::FileAccessDenied,
            "Cannot create variable file: " + path.string()});
    }

    auto dims = getImageDimensions(image);
    size_t numNodes = static_cast<size_t>(dims[0])
                      * static_cast<size_t>(dims[1])
                      * static_cast<size_t>(dims[2]);

    writeBinaryString(out, description);
    writeBinaryString(out, "part");
    writeBinaryInt(out, 1);
    writeBinaryString(out, "block");

    // Write values in block order (i fastest, then j, then k)
    const float* buf = image->GetBufferPointer();
    for (size_t i = 0; i < numNodes; ++i) {
        writeBinaryFloat(out, buf[i]);
    }

    return {};
}

// =============================================================================
// Vector variable writer (C Binary, per node)
// =============================================================================

std::expected<void, ExportError>
EnsightExporter::writeVectorVariable(const std::filesystem::path& path,
                                     const std::string& description,
                                     const VectorImage3D* image) {
    if (!image) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "Null image for variable: " + description});
    }

    if (image->GetNumberOfComponentsPerPixel() != 3) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "Vector image must have 3 components, got "
            + std::to_string(image->GetNumberOfComponentsPerPixel())});
    }

    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        return std::unexpected(ExportError{
            ExportError::Code::FileAccessDenied,
            "Cannot create variable file: " + path.string()});
    }

    auto size = image->GetLargestPossibleRegion().GetSize();
    int nx = static_cast<int>(size[0]);
    int ny = static_cast<int>(size[1]);
    int nz = static_cast<int>(size[2]);
    size_t numNodes = static_cast<size_t>(nx) * ny * nz;

    writeBinaryString(out, description);
    writeBinaryString(out, "part");
    writeBinaryInt(out, 1);
    writeBinaryString(out, "block");

    // Ensight vector block format: all Vx, then all Vy, then all Vz
    const float* buf = image->GetBufferPointer();

    for (int comp = 0; comp < 3; ++comp) {
        for (size_t i = 0; i < numNodes; ++i) {
            writeBinaryFloat(out, buf[i * 3 + comp]);
        }
    }

    return {};
}

// =============================================================================
// Binary I/O helpers
// =============================================================================

void EnsightExporter::writeBinaryString(std::ofstream& out,
                                        const std::string& str) {
    char buf[80] = {};
    size_t len = std::min(str.size(), size_t{79});
    std::memcpy(buf, str.data(), len);
    out.write(buf, 80);
}

void EnsightExporter::writeBinaryInt(std::ofstream& out, int32_t value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(int32_t));
}

void EnsightExporter::writeBinaryFloat(std::ofstream& out, float value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(float));
}

std::array<int, 3>
EnsightExporter::getImageDimensions(const FloatImage3D* image) {
    auto size = image->GetLargestPossibleRegion().GetSize();
    return {static_cast<int>(size[0]),
            static_cast<int>(size[1]),
            static_cast<int>(size[2])};
}

}  // namespace dicom_viewer::services

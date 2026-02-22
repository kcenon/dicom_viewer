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

#include "core/data_serializer.hpp"

#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace dicom_viewer::core {

// =============================================================================
// NRRD header helpers
// =============================================================================

namespace {

/**
 * @brief Build NRRD header + raw data for a 3D image
 *
 * NRRD format: text header terminated by blank line, followed by raw bytes.
 */
std::vector<uint8_t> buildNRRD(const std::string& type,
                               int dimension,
                               const std::vector<int>& sizes,
                               const std::vector<double>& spacings,
                               const std::array<double, 3>& origin,
                               const uint8_t* rawData,
                               size_t rawBytes) {
    std::ostringstream header;
    header << "NRRD0004\n";
    header << "type: " << type << "\n";
    header << "dimension: " << dimension << "\n";

    header << "sizes:";
    for (int s : sizes) header << " " << s;
    header << "\n";

    header << "spacings:";
    for (double sp : spacings) {
        header << " " << std::fixed << std::setprecision(6) << sp;
    }
    header << "\n";

    header << "space origin: ("
           << std::fixed << std::setprecision(6)
           << origin[0] << "," << origin[1] << "," << origin[2]
           << ")\n";

    header << "encoding: raw\n";
    header << "endian: little\n";
    header << "\n";  // Blank line terminates header

    std::string hdr = header.str();
    std::vector<uint8_t> result;
    result.reserve(hdr.size() + rawBytes);
    result.insert(result.end(), hdr.begin(), hdr.end());
    result.insert(result.end(), rawData, rawData + rawBytes);
    return result;
}

/**
 * @brief Parse NRRD header, returning header fields and offset to raw data
 */
struct NRRDHeader {
    std::string type;
    int dimension = 0;
    std::vector<int> sizes;
    std::vector<double> spacings;
    std::array<double, 3> origin = {0, 0, 0};
    size_t dataOffset = 0;
};

std::expected<NRRDHeader, ProjectError>
parseNRRDHeader(const std::vector<uint8_t>& data) {
    // Find the blank line that separates header from data
    std::string content(data.begin(), data.end());
    size_t headerEnd = content.find("\n\n");
    if (headerEnd == std::string::npos) {
        return std::unexpected(ProjectError::InvalidFormat);
    }

    NRRDHeader hdr;
    hdr.dataOffset = headerEnd + 2;  // Skip the two newlines

    // Parse header lines
    std::istringstream stream(content.substr(0, headerEnd));
    std::string line;

    // First line must be NRRD magic
    if (!std::getline(stream, line) || line.substr(0, 4) != "NRRD") {
        return std::unexpected(ProjectError::InvalidFormat);
    }

    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#') continue;

        auto colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;

        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);
        // Trim leading whitespace
        size_t start = value.find_first_not_of(" \t");
        if (start != std::string::npos) value = value.substr(start);

        if (key == "type") {
            hdr.type = value;
        } else if (key == "dimension") {
            hdr.dimension = std::stoi(value);
        } else if (key == "sizes") {
            std::istringstream ss(value);
            int s;
            while (ss >> s) hdr.sizes.push_back(s);
        } else if (key == "spacings") {
            std::istringstream ss(value);
            std::string token;
            while (ss >> token) {
                if (token == "nan" || token == "NaN" || token == "NAN") {
                    hdr.spacings.push_back(
                        std::numeric_limits<double>::quiet_NaN());
                } else {
                    try {
                        hdr.spacings.push_back(std::stod(token));
                    } catch (...) {
                        hdr.spacings.push_back(1.0);
                    }
                }
            }
        } else if (key == "space origin") {
            // Parse "(x,y,z)" format
            auto start = value.find('(');
            auto end = value.find(')');
            if (start != std::string::npos && end != std::string::npos) {
                std::string coords = value.substr(start + 1, end - start - 1);
                std::istringstream ss(coords);
                char comma;
                ss >> hdr.origin[0] >> comma
                   >> hdr.origin[1] >> comma
                   >> hdr.origin[2];
            }
        }
    }

    return hdr;
}

std::string formatPhaseIndex(int index) {
    std::ostringstream ss;
    ss << std::setw(4) << std::setfill('0') << index;
    return ss.str();
}

}  // anonymous namespace

// =============================================================================
// Scalar image NRRD
// =============================================================================

std::vector<uint8_t>
DataSerializer::scalarImageToNRRD(const FloatImage3D* image) {
    auto size = image->GetLargestPossibleRegion().GetSize();
    auto spacing = image->GetSpacing();
    auto origin = image->GetOrigin();

    std::vector<int> sizes = {
        static_cast<int>(size[0]),
        static_cast<int>(size[1]),
        static_cast<int>(size[2])
    };
    std::vector<double> spacings = {spacing[0], spacing[1], spacing[2]};
    std::array<double, 3> org = {origin[0], origin[1], origin[2]};

    size_t numVoxels = size[0] * size[1] * size[2];
    auto* buf = reinterpret_cast<const uint8_t*>(image->GetBufferPointer());

    return buildNRRD("float", 3, sizes, spacings, org,
                     buf, numVoxels * sizeof(float));
}

std::expected<DataSerializer::FloatImage3D::Pointer, ProjectError>
DataSerializer::nrrdToScalarImage(const std::vector<uint8_t>& data) {
    auto hdrResult = parseNRRDHeader(data);
    if (!hdrResult) return std::unexpected(hdrResult.error());
    auto& hdr = *hdrResult;

    if (hdr.type != "float" || hdr.dimension != 3 || hdr.sizes.size() != 3) {
        return std::unexpected(ProjectError::InvalidFormat);
    }

    size_t numVoxels = static_cast<size_t>(hdr.sizes[0])
                       * hdr.sizes[1] * hdr.sizes[2];
    size_t expectedBytes = numVoxels * sizeof(float);
    if (data.size() < hdr.dataOffset + expectedBytes) {
        return std::unexpected(ProjectError::InvalidFormat);
    }

    auto image = FloatImage3D::New();
    FloatImage3D::RegionType region;
    FloatImage3D::SizeType sz;
    sz[0] = hdr.sizes[0]; sz[1] = hdr.sizes[1]; sz[2] = hdr.sizes[2];
    region.SetSize(sz);
    image->SetRegions(region);

    FloatImage3D::SpacingType sp;
    sp[0] = hdr.spacings.size() > 0 ? hdr.spacings[0] : 1.0;
    sp[1] = hdr.spacings.size() > 1 ? hdr.spacings[1] : 1.0;
    sp[2] = hdr.spacings.size() > 2 ? hdr.spacings[2] : 1.0;
    image->SetSpacing(sp);

    FloatImage3D::PointType org;
    org[0] = hdr.origin[0]; org[1] = hdr.origin[1]; org[2] = hdr.origin[2];
    image->SetOrigin(org);

    image->Allocate();
    std::memcpy(image->GetBufferPointer(),
                data.data() + hdr.dataOffset, expectedBytes);

    return image;
}

// =============================================================================
// Vector image NRRD
// =============================================================================

std::vector<uint8_t>
DataSerializer::vectorImageToNRRD(const VectorImage3D* image) {
    auto size = image->GetLargestPossibleRegion().GetSize();
    auto spacing = image->GetSpacing();
    auto origin = image->GetOrigin();
    int numComponents = static_cast<int>(image->GetNumberOfComponentsPerPixel());

    std::vector<int> sizes = {
        numComponents,
        static_cast<int>(size[0]),
        static_cast<int>(size[1]),
        static_cast<int>(size[2])
    };
    // NaN for the component axis spacing (not spatial)
    std::vector<double> spacings = {
        std::numeric_limits<double>::quiet_NaN(),
        spacing[0], spacing[1], spacing[2]
    };
    std::array<double, 3> org = {origin[0], origin[1], origin[2]};

    size_t numVoxels = size[0] * size[1] * size[2];
    auto* buf = reinterpret_cast<const uint8_t*>(image->GetBufferPointer());

    return buildNRRD("float", 4, sizes, spacings, org,
                     buf, numVoxels * numComponents * sizeof(float));
}

std::expected<DataSerializer::VectorImage3D::Pointer, ProjectError>
DataSerializer::nrrdToVectorImage(const std::vector<uint8_t>& data) {
    auto hdrResult = parseNRRDHeader(data);
    if (!hdrResult) return std::unexpected(hdrResult.error());
    auto& hdr = *hdrResult;

    if (hdr.type != "float" || hdr.dimension != 4 || hdr.sizes.size() != 4) {
        return std::unexpected(ProjectError::InvalidFormat);
    }

    int numComponents = hdr.sizes[0];
    int nx = hdr.sizes[1], ny = hdr.sizes[2], nz = hdr.sizes[3];
    size_t numVoxels = static_cast<size_t>(nx) * ny * nz;
    size_t expectedBytes = numVoxels * numComponents * sizeof(float);

    if (data.size() < hdr.dataOffset + expectedBytes) {
        return std::unexpected(ProjectError::InvalidFormat);
    }

    auto image = VectorImage3D::New();
    VectorImage3D::RegionType region;
    VectorImage3D::SizeType sz;
    sz[0] = nx; sz[1] = ny; sz[2] = nz;
    region.SetSize(sz);
    image->SetRegions(region);
    image->SetNumberOfComponentsPerPixel(numComponents);

    VectorImage3D::SpacingType sp;
    sp[0] = hdr.spacings.size() > 1 ? hdr.spacings[1] : 1.0;
    sp[1] = hdr.spacings.size() > 2 ? hdr.spacings[2] : 1.0;
    sp[2] = hdr.spacings.size() > 3 ? hdr.spacings[3] : 1.0;
    image->SetSpacing(sp);

    VectorImage3D::PointType org;
    org[0] = hdr.origin[0]; org[1] = hdr.origin[1]; org[2] = hdr.origin[2];
    image->SetOrigin(org);

    image->Allocate();
    std::memcpy(image->GetBufferPointer(),
                data.data() + hdr.dataOffset, expectedBytes);

    return image;
}

// =============================================================================
// Label map NRRD
// =============================================================================

std::vector<uint8_t>
DataSerializer::labelMapToNRRD(const LabelMapType* image) {
    auto size = image->GetLargestPossibleRegion().GetSize();
    auto spacing = image->GetSpacing();
    auto origin = image->GetOrigin();

    std::vector<int> sizes = {
        static_cast<int>(size[0]),
        static_cast<int>(size[1]),
        static_cast<int>(size[2])
    };
    std::vector<double> spacings = {spacing[0], spacing[1], spacing[2]};
    std::array<double, 3> org = {origin[0], origin[1], origin[2]};

    size_t numVoxels = size[0] * size[1] * size[2];
    auto* buf = image->GetBufferPointer();

    return buildNRRD("unsigned char", 3, sizes, spacings, org,
                     buf, numVoxels);
}

std::expected<DataSerializer::LabelMapType::Pointer, ProjectError>
DataSerializer::nrrdToLabelMap(const std::vector<uint8_t>& data) {
    auto hdrResult = parseNRRDHeader(data);
    if (!hdrResult) return std::unexpected(hdrResult.error());
    auto& hdr = *hdrResult;

    if ((hdr.type != "unsigned char" && hdr.type != "uint8")
        || hdr.dimension != 3 || hdr.sizes.size() != 3) {
        return std::unexpected(ProjectError::InvalidFormat);
    }

    size_t numVoxels = static_cast<size_t>(hdr.sizes[0])
                       * hdr.sizes[1] * hdr.sizes[2];

    if (data.size() < hdr.dataOffset + numVoxels) {
        return std::unexpected(ProjectError::InvalidFormat);
    }

    auto image = LabelMapType::New();
    LabelMapType::RegionType region;
    LabelMapType::SizeType sz;
    sz[0] = hdr.sizes[0]; sz[1] = hdr.sizes[1]; sz[2] = hdr.sizes[2];
    region.SetSize(sz);
    image->SetRegions(region);

    LabelMapType::SpacingType sp;
    sp[0] = hdr.spacings.size() > 0 ? hdr.spacings[0] : 1.0;
    sp[1] = hdr.spacings.size() > 1 ? hdr.spacings[1] : 1.0;
    sp[2] = hdr.spacings.size() > 2 ? hdr.spacings[2] : 1.0;
    image->SetSpacing(sp);

    LabelMapType::PointType org;
    org[0] = hdr.origin[0]; org[1] = hdr.origin[1]; org[2] = hdr.origin[2];
    image->SetOrigin(org);

    image->Allocate();
    std::memcpy(image->GetBufferPointer(),
                data.data() + hdr.dataOffset, numVoxels);

    return image;
}

// =============================================================================
// High-level ZIP serialization
// =============================================================================

std::expected<void, ProjectError>
DataSerializer::saveVelocityData(
    ZipArchive& zip,
    const std::vector<VectorImage3D::Pointer>& velocityPhases,
    const std::vector<FloatImage3D::Pointer>& magnitudePhases) {

    for (size_t i = 0; i < velocityPhases.size(); ++i) {
        if (!velocityPhases[i]) {
            return std::unexpected(ProjectError::SerializationError);
        }
        std::string idx = formatPhaseIndex(static_cast<int>(i));
        zip.addEntry("data/velocity/phase_" + idx + ".nrrd",
                     vectorImageToNRRD(velocityPhases[i].GetPointer()));
    }

    for (size_t i = 0; i < magnitudePhases.size(); ++i) {
        if (!magnitudePhases[i]) {
            return std::unexpected(ProjectError::SerializationError);
        }
        std::string idx = formatPhaseIndex(static_cast<int>(i));
        zip.addEntry("data/magnitude/phase_" + idx + ".nrrd",
                     scalarImageToNRRD(magnitudePhases[i].GetPointer()));
    }

    // Store phase count for loading
    nlohmann::json meta = {
        {"velocity_count", static_cast<int>(velocityPhases.size())},
        {"magnitude_count", static_cast<int>(magnitudePhases.size())}
    };
    zip.addEntry("data/velocity/meta.json", meta.dump(2));

    return {};
}

std::expected<void, ProjectError>
DataSerializer::loadVelocityData(
    const ZipArchive& zip,
    std::vector<VectorImage3D::Pointer>& velocityPhases,
    std::vector<FloatImage3D::Pointer>& magnitudePhases) {

    if (!zip.hasEntry("data/velocity/meta.json")) {
        return std::unexpected(ProjectError::SerializationError);
    }

    auto metaStr = zip.readEntryAsString("data/velocity/meta.json");
    if (!metaStr) {
        return std::unexpected(ProjectError::SerializationError);
    }

    nlohmann::json meta;
    try {
        meta = nlohmann::json::parse(*metaStr);
    } catch (...) {
        return std::unexpected(ProjectError::InvalidFormat);
    }

    int velCount = meta.value("velocity_count", 0);
    int magCount = meta.value("magnitude_count", 0);

    velocityPhases.clear();
    velocityPhases.reserve(velCount);

    for (int i = 0; i < velCount; ++i) {
        std::string idx = formatPhaseIndex(i);
        std::string path = "data/velocity/phase_" + idx + ".nrrd";
        auto entry = zip.readEntry(path);
        if (!entry) {
            return std::unexpected(ProjectError::SerializationError);
        }
        auto img = nrrdToVectorImage(*entry);
        if (!img) return std::unexpected(img.error());
        velocityPhases.push_back(*img);
    }

    magnitudePhases.clear();
    magnitudePhases.reserve(magCount);

    for (int i = 0; i < magCount; ++i) {
        std::string idx = formatPhaseIndex(i);
        std::string path = "data/magnitude/phase_" + idx + ".nrrd";
        auto entry = zip.readEntry(path);
        if (!entry) {
            return std::unexpected(ProjectError::SerializationError);
        }
        auto img = nrrdToScalarImage(*entry);
        if (!img) return std::unexpected(img.error());
        magnitudePhases.push_back(*img);
    }

    return {};
}

std::expected<void, ProjectError>
DataSerializer::saveMask(ZipArchive& zip,
                         const LabelMapType* labelMap,
                         const std::vector<LabelDefinition>& labels) {
    if (!labelMap) {
        return std::unexpected(ProjectError::SerializationError);
    }

    zip.addEntry("data/mask/label_map.nrrd", labelMapToNRRD(labelMap));

    nlohmann::json labelsJson = nlohmann::json::array();
    for (const auto& label : labels) {
        labelsJson.push_back({
            {"id", label.id},
            {"name", label.name},
            {"color", {label.color[0], label.color[1], label.color[2]}},
            {"opacity", label.opacity}
        });
    }
    zip.addEntry("data/mask/labels.json", labelsJson.dump(2));

    return {};
}

std::expected<void, ProjectError>
DataSerializer::loadMask(const ZipArchive& zip,
                         LabelMapType::Pointer& labelMap,
                         std::vector<LabelDefinition>& labels) {
    if (!zip.hasEntry("data/mask/label_map.nrrd")) {
        return std::unexpected(ProjectError::SerializationError);
    }

    auto nrrdData = zip.readEntry("data/mask/label_map.nrrd");
    if (!nrrdData) {
        return std::unexpected(ProjectError::SerializationError);
    }
    auto img = nrrdToLabelMap(*nrrdData);
    if (!img) return std::unexpected(img.error());
    labelMap = *img;

    labels.clear();
    if (zip.hasEntry("data/mask/labels.json")) {
        auto labelsStr = zip.readEntryAsString("data/mask/labels.json");
        if (labelsStr) {
            try {
                auto j = nlohmann::json::parse(*labelsStr);
                for (const auto& item : j) {
                    LabelDefinition def;
                    def.id = item.value("id", uint8_t{0});
                    def.name = item.value("name", "");
                    if (item.contains("color") && item["color"].is_array()
                        && item["color"].size() == 3) {
                        def.color[0] = item["color"][0].get<float>();
                        def.color[1] = item["color"][1].get<float>();
                        def.color[2] = item["color"][2].get<float>();
                    }
                    def.opacity = item.value("opacity", 1.0f);
                    labels.push_back(def);
                }
            } catch (...) {
                // Labels metadata is optional; continue without it
            }
        }
    }

    return {};
}

std::expected<void, ProjectError>
DataSerializer::saveAnalysisResults(ZipArchive& zip,
                                    const nlohmann::json& results) {
    zip.addEntry("data/analysis/results.json", results.dump(2));
    return {};
}

std::expected<nlohmann::json, ProjectError>
DataSerializer::loadAnalysisResults(const ZipArchive& zip) {
    if (!zip.hasEntry("data/analysis/results.json")) {
        return std::unexpected(ProjectError::SerializationError);
    }

    auto str = zip.readEntryAsString("data/analysis/results.json");
    if (!str) {
        return std::unexpected(ProjectError::SerializationError);
    }

    try {
        return nlohmann::json::parse(*str);
    } catch (...) {
        return std::unexpected(ProjectError::InvalidFormat);
    }
}

}  // namespace dicom_viewer::core

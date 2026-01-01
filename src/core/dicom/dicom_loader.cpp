#include "core/dicom_loader.hpp"
#include "core/transfer_syntax_decoder.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>

#include <itkGDCMImageIO.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkImageSeriesReader.h>
#include <itkImageFileReader.h>
#include <itkMetaDataObject.h>

namespace dicom_viewer::core {

class DicomLoader::Impl {
public:
    itk::GDCMImageIO::Pointer gdcmIO;

    Impl() : gdcmIO(itk::GDCMImageIO::New()) {}
};

DicomLoader::DicomLoader() : impl_(std::make_unique<Impl>()) {}

DicomLoader::~DicomLoader() = default;

DicomLoader::DicomLoader(DicomLoader&&) noexcept = default;
DicomLoader& DicomLoader::operator=(DicomLoader&&) noexcept = default;

std::expected<DicomMetadata, DicomErrorInfo>
DicomLoader::loadFile(const std::filesystem::path& filePath)
{
    if (!std::filesystem::exists(filePath)) {
        return std::unexpected(DicomErrorInfo{
            DicomError::FileNotFound,
            "File not found: " + filePath.string()
        });
    }

    try {
        using ReaderType = itk::ImageFileReader<itk::Image<short, 2>>;
        auto reader = ReaderType::New();
        reader->SetFileName(filePath.string());
        reader->SetImageIO(impl_->gdcmIO);
        reader->Update();

        // Extract metadata from GDCM dictionary
        const auto& dictionary = impl_->gdcmIO->GetMetaDataDictionary();

        auto getString = [&dictionary](const std::string& key) -> std::string {
            std::string value;
            itk::ExposeMetaData<std::string>(dictionary, key, value);
            return value;
        };

        metadata_.patientName = getString("0010|0010");
        metadata_.patientId = getString("0010|0020");
        metadata_.patientBirthDate = getString("0010|0030");
        metadata_.patientSex = getString("0010|0040");

        metadata_.studyInstanceUid = getString("0020|000d");
        metadata_.studyDate = getString("0008|0020");
        metadata_.studyTime = getString("0008|0030");
        metadata_.studyDescription = getString("0008|1030");
        metadata_.accessionNumber = getString("0008|0050");

        metadata_.seriesInstanceUid = getString("0020|000e");
        metadata_.seriesNumber = getString("0020|0011");
        metadata_.seriesDescription = getString("0008|103e");
        metadata_.modality = getString("0008|0060");

        metadata_.rows = impl_->gdcmIO->GetDimensions(1);
        metadata_.columns = impl_->gdcmIO->GetDimensions(0);
        metadata_.bitsAllocated = impl_->gdcmIO->GetComponentSize() * 8;

        // Pixel spacing
        auto spacing = impl_->gdcmIO->GetSpacing(0);
        metadata_.pixelSpacingX = impl_->gdcmIO->GetSpacing(0);
        metadata_.pixelSpacingY = impl_->gdcmIO->GetSpacing(1);

        // Rescale parameters
        metadata_.rescaleSlope = impl_->gdcmIO->GetRescaleSlope();
        metadata_.rescaleIntercept = impl_->gdcmIO->GetRescaleIntercept();

        return metadata_;

    } catch (const itk::ExceptionObject& e) {
        return std::unexpected(DicomErrorInfo{
            DicomError::InvalidDicomFormat,
            std::string("Failed to load DICOM file: ") + e.what()
        });
    }
}

namespace {

/// Parse a DICOM multi-value string (backslash-separated) into doubles
std::vector<double> parseMultiValueDouble(const std::string& str)
{
    std::vector<double> values;
    if (str.empty()) {
        return values;
    }

    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, '\\')) {
        try {
            values.push_back(std::stod(token));
        } catch (...) {
            values.push_back(0.0);
        }
    }
    return values;
}

/// Extract slice information from DICOM metadata dictionary
SliceInfo extractSliceInfo(const std::filesystem::path& filePath,
                           itk::GDCMImageIO::Pointer gdcmIO)
{
    SliceInfo info;
    info.filePath = filePath;

    using ReaderType = itk::ImageFileReader<itk::Image<short, 2>>;
    auto reader = ReaderType::New();
    reader->SetFileName(filePath.string());
    reader->SetImageIO(gdcmIO);

    try {
        reader->Update();
    } catch (...) {
        return info;
    }

    const auto& dictionary = gdcmIO->GetMetaDataDictionary();

    auto getString = [&dictionary](const std::string& key) -> std::string {
        std::string value;
        itk::ExposeMetaData<std::string>(dictionary, key, value);
        // Trim whitespace
        size_t start = value.find_first_not_of(" \t\r\n");
        size_t end = value.find_last_not_of(" \t\r\n");
        if (start != std::string::npos && end != std::string::npos) {
            return value.substr(start, end - start + 1);
        }
        return "";
    };

    // Image Position Patient (0020,0032)
    std::string positionStr = getString("0020|0032");
    auto positionValues = parseMultiValueDouble(positionStr);
    if (positionValues.size() >= 3) {
        info.imagePosition[0] = positionValues[0];
        info.imagePosition[1] = positionValues[1];
        info.imagePosition[2] = positionValues[2];
    }

    // Image Orientation Patient (0020,0037)
    std::string orientationStr = getString("0020|0037");
    auto orientationValues = parseMultiValueDouble(orientationStr);
    if (orientationValues.size() >= 6) {
        for (size_t i = 0; i < 6; ++i) {
            info.imageOrientation[i] = orientationValues[i];
        }
    }

    // Slice Location (0020,1041)
    std::string sliceLocStr = getString("0020|1041");
    if (!sliceLocStr.empty()) {
        try {
            info.sliceLocation = std::stod(sliceLocStr);
        } catch (...) {
            info.sliceLocation = 0.0;
        }
    }

    // Instance Number (0020,0013)
    std::string instanceStr = getString("0020|0013");
    if (!instanceStr.empty()) {
        try {
            info.instanceNumber = std::stoi(instanceStr);
        } catch (...) {
            info.instanceNumber = 0;
        }
    }

    return info;
}

} // anonymous namespace

std::expected<std::map<std::string, std::vector<SliceInfo>>, DicomErrorInfo>
DicomLoader::scanDirectory(const std::filesystem::path& directoryPath)
{
    if (!std::filesystem::exists(directoryPath) ||
        !std::filesystem::is_directory(directoryPath)) {
        return std::unexpected(DicomErrorInfo{
            DicomError::FileNotFound,
            "Directory not found: " + directoryPath.string()
        });
    }

    try {
        using NamesGeneratorType = itk::GDCMSeriesFileNames;
        auto namesGenerator = NamesGeneratorType::New();
        namesGenerator->SetUseSeriesDetails(true);
        namesGenerator->SetRecursive(false);
        namesGenerator->SetDirectory(directoryPath.string());

        std::map<std::string, std::vector<SliceInfo>> seriesMap;

        const auto& seriesUIDs = namesGenerator->GetSeriesUIDs();
        for (const auto& uid : seriesUIDs) {
            const auto& fileNames = namesGenerator->GetFileNames(uid);
            std::vector<SliceInfo> slices;
            slices.reserve(fileNames.size());

            for (const auto& fileName : fileNames) {
                auto gdcmIO = itk::GDCMImageIO::New();
                SliceInfo info = extractSliceInfo(fileName, gdcmIO);
                slices.push_back(std::move(info));
            }

            sortSlices(slices);
            seriesMap[uid] = std::move(slices);
        }

        return seriesMap;

    } catch (const std::exception& e) {
        return std::unexpected(DicomErrorInfo{
            DicomError::SeriesAssemblyFailed,
            std::string("Failed to scan directory: ") + e.what()
        });
    }
}

std::expected<CTImageType::Pointer, DicomErrorInfo>
DicomLoader::loadCTSeries(const std::vector<SliceInfo>& slices)
{
    if (slices.empty()) {
        return std::unexpected(DicomErrorInfo{
            DicomError::SeriesAssemblyFailed,
            "No slices provided"
        });
    }

    try {
        std::vector<std::string> fileNames;
        fileNames.reserve(slices.size());
        for (const auto& slice : slices) {
            fileNames.push_back(slice.filePath.string());
        }

        using ReaderType = itk::ImageSeriesReader<CTImageType>;
        auto reader = ReaderType::New();
        reader->SetImageIO(impl_->gdcmIO);
        reader->SetFileNames(fileNames);
        reader->Update();

        // Store metadata from first slice
        loadFile(slices.front().filePath);

        return reader->GetOutput();

    } catch (const itk::ExceptionObject& e) {
        return std::unexpected(DicomErrorInfo{
            DicomError::SeriesAssemblyFailed,
            std::string("Failed to load CT series: ") + e.what()
        });
    }
}

std::expected<MRImageType::Pointer, DicomErrorInfo>
DicomLoader::loadMRSeries(const std::vector<SliceInfo>& slices)
{
    if (slices.empty()) {
        return std::unexpected(DicomErrorInfo{
            DicomError::SeriesAssemblyFailed,
            "No slices provided"
        });
    }

    try {
        std::vector<std::string> fileNames;
        fileNames.reserve(slices.size());
        for (const auto& slice : slices) {
            fileNames.push_back(slice.filePath.string());
        }

        using ReaderType = itk::ImageSeriesReader<MRImageType>;
        auto reader = ReaderType::New();
        reader->SetImageIO(impl_->gdcmIO);
        reader->SetFileNames(fileNames);
        reader->Update();

        loadFile(slices.front().filePath);

        return reader->GetOutput();

    } catch (const itk::ExceptionObject& e) {
        return std::unexpected(DicomErrorInfo{
            DicomError::SeriesAssemblyFailed,
            std::string("Failed to load MR series: ") + e.what()
        });
    }
}

void DicomLoader::sortSlices(std::vector<SliceInfo>& slices)
{
    if (slices.empty()) {
        return;
    }

    // Calculate slice normal from first slice's orientation
    // orientation = [row_x, row_y, row_z, col_x, col_y, col_z]
    // normal = row × col
    const auto& orient = slices.front().imageOrientation;
    std::array<double, 3> normal = {
        orient[1] * orient[5] - orient[2] * orient[4],  // row_y * col_z - row_z * col_y
        orient[2] * orient[3] - orient[0] * orient[5],  // row_z * col_x - row_x * col_z
        orient[0] * orient[4] - orient[1] * orient[3]   // row_x * col_y - row_y * col_x
    };

    // Calculate position along normal for each slice
    for (auto& slice : slices) {
        slice.sliceLocation = calculateSlicePosition(slice, normal);
    }

    // Check if Image Position Patient is available and varies
    bool hasValidPosition = false;
    if (slices.size() >= 2) {
        double firstPos = slices[0].sliceLocation;
        double lastPos = slices[slices.size() - 1].sliceLocation;
        hasValidPosition = std::abs(firstPos - lastPos) > 1e-6;
    }

    std::sort(slices.begin(), slices.end(),
        [hasValidPosition](const SliceInfo& a, const SliceInfo& b) {
            constexpr double epsilon = 1e-6;

            // Primary: position along slice normal (calculated from Image Position Patient)
            if (hasValidPosition && std::abs(a.sliceLocation - b.sliceLocation) > epsilon) {
                return a.sliceLocation < b.sliceLocation;
            }

            // Fallback 1: Instance Number
            if (a.instanceNumber != b.instanceNumber) {
                return a.instanceNumber < b.instanceNumber;
            }

            // Fallback 2: File path (for deterministic ordering)
            return a.filePath < b.filePath;
        });
}

double DicomLoader::calculateSlicePosition(const SliceInfo& slice)
{
    // Default normal for axial orientation
    std::array<double, 3> normal = {0.0, 0.0, 1.0};
    return calculateSlicePosition(slice, normal);
}

double DicomLoader::calculateSlicePosition(const SliceInfo& slice,
                                           const std::array<double, 3>& normal)
{
    // Project image position onto the slice normal to get position along stack
    return slice.imagePosition[0] * normal[0] +
           slice.imagePosition[1] * normal[1] +
           slice.imagePosition[2] * normal[2];
}

bool DicomLoader::isTransferSyntaxSupported(const std::string& uid)
{
    return TransferSyntaxDecoder::isSupported(uid);
}

std::vector<std::string> DicomLoader::getSupportedTransferSyntaxes()
{
    return TransferSyntaxDecoder::getSupportedUIDs();
}

} // namespace dicom_viewer::core

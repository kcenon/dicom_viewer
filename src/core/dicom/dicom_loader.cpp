#include "core/dicom_loader.hpp"

#include <algorithm>
#include <map>

#include <itkGDCMImageIO.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkImageSeriesReader.h>
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
        namesGenerator->SetDirectory(directoryPath.string());

        std::map<std::string, std::vector<SliceInfo>> seriesMap;

        const auto& seriesUIDs = namesGenerator->GetSeriesUIDs();
        for (const auto& uid : seriesUIDs) {
            const auto& fileNames = namesGenerator->GetFileNames(uid);
            std::vector<SliceInfo> slices;

            for (const auto& fileName : fileNames) {
                SliceInfo info;
                info.filePath = fileName;

                // Load metadata for sorting
                auto result = loadFile(fileName);
                if (result) {
                    // Get slice location from image position
                    // This is a simplified version; full implementation
                    // would parse Image Position Patient tag
                    info.instanceNumber = std::stoi(
                        result->seriesNumber.empty() ? "0" : result->seriesNumber
                    );
                }

                slices.push_back(info);
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
    std::sort(slices.begin(), slices.end(),
        [](const SliceInfo& a, const SliceInfo& b) {
            // Primary: slice location
            if (a.sliceLocation != b.sliceLocation) {
                return a.sliceLocation < b.sliceLocation;
            }
            // Fallback: instance number
            return a.instanceNumber < b.instanceNumber;
        });
}

double DicomLoader::calculateSlicePosition(const SliceInfo& slice)
{
    // Calculate position along slice normal
    // For standard orientations, this is typically the Z component
    // of Image Position Patient
    return slice.imagePosition[2];
}

bool DicomLoader::isTransferSyntaxSupported(const std::string& uid)
{
    static const std::vector<std::string> supported = {
        "1.2.840.10008.1.2",       // Implicit VR Little Endian
        "1.2.840.10008.1.2.1",     // Explicit VR Little Endian
        "1.2.840.10008.1.2.4.50",  // JPEG Baseline
        "1.2.840.10008.1.2.4.70",  // JPEG Lossless
        "1.2.840.10008.1.2.4.90",  // JPEG 2000 Lossless
        "1.2.840.10008.1.2.4.91",  // JPEG 2000
        "1.2.840.10008.1.2.4.80",  // JPEG-LS Lossless
        "1.2.840.10008.1.2.5"      // RLE Lossless
    };

    return std::find(supported.begin(), supported.end(), uid) != supported.end();
}

std::vector<std::string> DicomLoader::getSupportedTransferSyntaxes()
{
    return {
        "1.2.840.10008.1.2",
        "1.2.840.10008.1.2.1",
        "1.2.840.10008.1.2.4.50",
        "1.2.840.10008.1.2.4.70",
        "1.2.840.10008.1.2.4.90",
        "1.2.840.10008.1.2.4.91",
        "1.2.840.10008.1.2.4.80",
        "1.2.840.10008.1.2.5"
    };
}

} // namespace dicom_viewer::core

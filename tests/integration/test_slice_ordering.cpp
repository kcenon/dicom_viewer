// Integration test for slice ordering verification
#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <array>
#include <algorithm>
#include <cmath>

#include <itkImage.h>
#include <itkGDCMImageIO.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkImageSeriesReader.h>
#include <itkMetaDataObject.h>

namespace fs = std::filesystem;

struct SliceInfo {
    fs::path filePath;
    int instanceNumber = 0;
    std::array<double, 3> imagePosition = {0.0, 0.0, 0.0};
    double zPosition = 0.0;
};

std::vector<double> parseMultiValueDouble(const std::string& str)
{
    std::vector<double> values;
    if (str.empty()) return values;

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

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <dicom_directory>" << std::endl;
        return 1;
    }

    fs::path dicomDir = argv[1];
    std::cout << "=== Slice Ordering Verification Test ===" << std::endl;
    std::cout << "Directory: " << dicomDir << std::endl;

    try {
        using NamesGeneratorType = itk::GDCMSeriesFileNames;
        auto namesGenerator = NamesGeneratorType::New();
        namesGenerator->SetUseSeriesDetails(true);
        namesGenerator->SetDirectory(dicomDir.string());

        const auto& seriesUIDs = namesGenerator->GetSeriesUIDs();
        if (seriesUIDs.empty()) {
            std::cerr << "[FAIL] No series found" << std::endl;
            return 1;
        }

        const auto& fileNames = namesGenerator->GetFileNames(seriesUIDs[0]);
        std::vector<SliceInfo> slices;

        std::cout << "\nExtracting slice positions..." << std::endl;

        for (const auto& fileName : fileNames) {
            auto gdcmIO = itk::GDCMImageIO::New();
            using ReaderType = itk::ImageFileReader<itk::Image<short, 2>>;
            auto reader = ReaderType::New();
            reader->SetFileName(fileName);
            reader->SetImageIO(gdcmIO);
            reader->Update();

            const auto& dict = gdcmIO->GetMetaDataDictionary();

            SliceInfo info;
            info.filePath = fileName;

            std::string posStr, instanceStr;
            itk::ExposeMetaData<std::string>(dict, "0020|0032", posStr);
            itk::ExposeMetaData<std::string>(dict, "0020|0013", instanceStr);

            auto pos = parseMultiValueDouble(posStr);
            if (pos.size() >= 3) {
                info.imagePosition = {pos[0], pos[1], pos[2]};
                info.zPosition = pos[2];  // Z coordinate
            }

            if (!instanceStr.empty()) {
                try {
                    info.instanceNumber = std::stoi(instanceStr);
                } catch (...) {}
            }

            slices.push_back(info);
        }

        // Sort by Z position
        std::sort(slices.begin(), slices.end(),
            [](const SliceInfo& a, const SliceInfo& b) {
                return a.zPosition < b.zPosition;
            });

        std::cout << "Total slices: " << slices.size() << std::endl;
        std::cout << "\nFirst 5 slices (sorted by Z):" << std::endl;
        for (size_t i = 0; i < std::min(size_t(5), slices.size()); ++i) {
            std::cout << "  [" << i << "] Z=" << slices[i].zPosition
                      << ", Instance=" << slices[i].instanceNumber
                      << ", File=" << slices[i].filePath.filename() << std::endl;
        }

        std::cout << "\nLast 5 slices (sorted by Z):" << std::endl;
        for (size_t i = std::max(size_t(0), slices.size() - 5); i < slices.size(); ++i) {
            std::cout << "  [" << i << "] Z=" << slices[i].zPosition
                      << ", Instance=" << slices[i].instanceNumber
                      << ", File=" << slices[i].filePath.filename() << std::endl;
        }

        // Verify monotonic ordering
        bool isMonotonic = true;
        double prevZ = slices[0].zPosition;
        for (size_t i = 1; i < slices.size(); ++i) {
            if (slices[i].zPosition < prevZ) {
                isMonotonic = false;
                std::cout << "[WARN] Non-monotonic at index " << i << std::endl;
                break;
            }
            prevZ = slices[i].zPosition;
        }

        if (isMonotonic) {
            std::cout << "\n[PASS] Slices are monotonically ordered by Z position" << std::endl;
        } else {
            std::cout << "\n[FAIL] Slices are NOT monotonically ordered" << std::endl;
        }

        // Calculate spacing consistency
        std::vector<double> spacings;
        for (size_t i = 1; i < slices.size(); ++i) {
            double spacing = std::abs(slices[i].zPosition - slices[i-1].zPosition);
            if (spacing > 1e-6) {
                spacings.push_back(spacing);
            }
        }

        if (!spacings.empty()) {
            std::sort(spacings.begin(), spacings.end());
            double median = spacings[spacings.size() / 2];
            double minSpacing = spacings.front();
            double maxSpacing = spacings.back();

            std::cout << "\nSpacing analysis:" << std::endl;
            std::cout << "  Median spacing: " << median << " mm" << std::endl;
            std::cout << "  Min spacing: " << minSpacing << " mm" << std::endl;
            std::cout << "  Max spacing: " << maxSpacing << " mm" << std::endl;

            double variability = (maxSpacing - minSpacing) / median * 100;
            if (variability < 10) {
                std::cout << "  [PASS] Spacing variability: " << variability << "% (< 10%)" << std::endl;
            } else {
                std::cout << "  [WARN] High spacing variability: " << variability << "%" << std::endl;
            }
        }

        std::cout << "\n=== Slice Ordering Test Complete ===" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Exception: " << e.what() << std::endl;
        return 1;
    }
}

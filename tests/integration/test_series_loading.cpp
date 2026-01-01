// Integration test for DICOM series loading
// Build: c++ -std=c++23 -I../../include -I/opt/homebrew/include/ITK-5.4 \
//        $(pkg-config --cflags --libs ITK-5.4) -o test_series_loading test_series_loading.cpp

#include <iostream>
#include <filesystem>
#include <map>
#include <vector>
#include <string>
#include <array>

#include <itkImage.h>
#include <itkGDCMImageIO.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkImageSeriesReader.h>
#include <itkMetaDataObject.h>

namespace fs = std::filesystem;

struct SliceInfo {
    fs::path filePath;
    double sliceLocation = 0.0;
    int instanceNumber = 0;
    std::array<double, 3> imagePosition = {0.0, 0.0, 0.0};
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
    if (!fs::exists(dicomDir)) {
        std::cerr << "Directory not found: " << dicomDir << std::endl;
        return 1;
    }

    std::cout << "=== DICOM Series Loading Test ===" << std::endl;
    std::cout << "Directory: " << dicomDir << std::endl;

    try {
        // Step 1: Scan directory for series
        using NamesGeneratorType = itk::GDCMSeriesFileNames;
        auto namesGenerator = NamesGeneratorType::New();
        namesGenerator->SetUseSeriesDetails(true);
        namesGenerator->SetRecursive(false);
        namesGenerator->SetDirectory(dicomDir.string());

        const auto& seriesUIDs = namesGenerator->GetSeriesUIDs();
        std::cout << "\n[PASS] Found " << seriesUIDs.size() << " series" << std::endl;

        for (const auto& uid : seriesUIDs) {
            const auto& fileNames = namesGenerator->GetFileNames(uid);
            std::cout << "\nSeries UID: " << uid.substr(0, 40) << "..." << std::endl;
            std::cout << "  Slice count: " << fileNames.size() << std::endl;

            // Step 2: Extract slice info from first and last slice
            auto gdcmIO = itk::GDCMImageIO::New();

            using ReaderType = itk::ImageFileReader<itk::Image<short, 2>>;
            auto reader = ReaderType::New();
            reader->SetFileName(fileNames.front());
            reader->SetImageIO(gdcmIO);
            reader->Update();

            const auto& dict = gdcmIO->GetMetaDataDictionary();
            std::string posStr, modality, seriesDesc;
            itk::ExposeMetaData<std::string>(dict, "0020|0032", posStr);
            itk::ExposeMetaData<std::string>(dict, "0008|0060", modality);
            itk::ExposeMetaData<std::string>(dict, "0008|103e", seriesDesc);

            auto pos = parseMultiValueDouble(posStr);
            if (pos.size() >= 3) {
                std::cout << "  First slice position: [" << pos[0] << ", " << pos[1] << ", " << pos[2] << "]" << std::endl;
            }
            std::cout << "  Modality: " << modality << std::endl;
            std::cout << "  Description: " << seriesDesc << std::endl;

            // Step 3: Load as 3D volume
            if (fileNames.size() >= 2) {
                std::cout << "  Loading 3D volume..." << std::flush;

                using ImageType = itk::Image<short, 3>;
                using SeriesReaderType = itk::ImageSeriesReader<ImageType>;
                auto seriesReader = SeriesReaderType::New();
                seriesReader->SetImageIO(itk::GDCMImageIO::New());
                seriesReader->SetFileNames(fileNames);
                seriesReader->Update();

                auto image = seriesReader->GetOutput();
                auto size = image->GetLargestPossibleRegion().GetSize();
                auto spacing = image->GetSpacing();

                std::cout << " [PASS]" << std::endl;
                std::cout << "  Volume size: " << size[0] << " x " << size[1] << " x " << size[2] << std::endl;
                std::cout << "  Spacing: " << spacing[0] << " x " << spacing[1] << " x " << spacing[2] << " mm" << std::endl;

                // Calculate slice spacing
                if (fileNames.size() >= 2) {
                    double sliceSpacing = spacing[2];
                    std::cout << "  Calculated slice spacing: " << sliceSpacing << " mm" << std::endl;
                }

                std::cout << "[PASS] 3D volume generation successful" << std::endl;
            }
        }

        std::cout << "\n=== All Tests Passed ===" << std::endl;
        return 0;

    } catch (const itk::ExceptionObject& e) {
        std::cerr << "[FAIL] ITK Exception: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Exception: " << e.what() << std::endl;
        return 1;
    }
}

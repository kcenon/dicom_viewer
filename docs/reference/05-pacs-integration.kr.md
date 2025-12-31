# pacs_system 연동 가이드

> **Last Updated**: 2025-12-31
> **pacs_system Location**: `/Users/dongcheolshin/Sources/pacs_system`
> **Version**: Tier 5 (kcenon ecosystem)

## 1. 개요

### 1.1 pacs_system 아키텍처

`pacs_system`은 kcenon 에코시스템의 DICOM 처리 모듈로, 외부 DICOM 라이브러리(DCMTK, GDCM 등) 없이 순수 C++20으로 구현된 완전한 PACS 솔루션입니다.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                       pacs_system Architecture                          │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐  │
│   │                     Application Layer                            │  │
│   │  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐            │  │
│   │  │  Web    │  │   AI    │  │Workflow │  │ Monitor │            │  │
│   │  │  API    │  │ Service │  │ Service │  │ Service │            │  │
│   │  └─────────┘  └─────────┘  └─────────┘  └─────────┘            │  │
│   └─────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐  │
│   │                      Service Layer                               │  │
│   │  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐            │  │
│   │  │ Storage │  │  Query  │  │ Retrieve│  │Worklist │            │  │
│   │  │   SCP   │  │   SCP   │  │   SCP   │  │   SCP   │            │  │
│   │  └─────────┘  └─────────┘  └─────────┘  └─────────┘            │  │
│   │  ┌─────────┐  ┌─────────┐  ┌─────────┐                         │  │
│   │  │  Echo   │  │  MPPS   │  │Security │                         │  │
│   │  │   SCP   │  │   SCP   │  │ Module  │                         │  │
│   │  └─────────┘  └─────────┘  └─────────┘                         │  │
│   └─────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐  │
│   │                       Core Layer                                 │  │
│   │  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐            │  │
│   │  │  DICOM  │  │ Encoding│  │ Network │  │ Storage │            │  │
│   │  │  Core   │  │  /Codec │  │  (PDU)  │  │ Backend │            │  │
│   │  └─────────┘  └─────────┘  └─────────┘  └─────────┘            │  │
│   └─────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐  │
│   │                   Ecosystem Integration                          │  │
│   │  common_system → container_system → thread_system → network_sys │  │
│   │       (Tier 0)        (Tier 1)         (Tier 1)      (Tier 4)   │  │
│   └─────────────────────────────────────────────────────────────────┘  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 1.2 주요 모듈

| 모듈 | 헤더 위치 | 기능 |
|------|-----------|------|
| **core** | `pacs/core/` | DICOM 데이터 구조 |
| **encoding** | `pacs/encoding/` | VR 인코딩, 압축 코덱 |
| **network** | `pacs/network/` | PDU, Association, DIMSE |
| **services** | `pacs/services/` | SCP/SCU 구현 |
| **storage** | `pacs/storage/` | 파일/클라우드 저장 |
| **security** | `pacs/security/` | RBAC, 익명화 |
| **integration** | `pacs/integration/` | 시스템 어댑터 |

---

## 2. DICOM 파일 처리

### 2.1 핵심 클래스

```cpp
// pacs_system의 핵심 DICOM 클래스들
#include <pacs/core/dicom_file.hpp>
#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_element.hpp>
#include <pacs/core/dicom_tag.hpp>
#include <pacs/core/dicom_dictionary.hpp>
```

### 2.2 DICOM 파일 읽기

```cpp
#include <pacs/core/dicom_file.hpp>

using namespace pacs;

// 파일에서 읽기
auto result = dicom_file::open("/path/to/image.dcm");
if (!result) {
    std::cerr << "Error: " << result.error().message() << std::endl;
    return;
}

dicom_file file = std::move(result.value());

// 데이터셋 접근
const dicom_dataset& dataset = file.dataset();

// 태그 값 읽기
if (auto element = dataset.find(dicom_tag::patient_name)) {
    std::string patientName = element->get_string();
}

if (auto element = dataset.find(dicom_tag::pixel_data)) {
    std::span<const std::byte> pixelData = element->get_bytes();
}
```

### 2.3 메타데이터 접근

```cpp
#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_tag.hpp>

// 주요 태그 상수
namespace dicom_tag {
    // Patient Module
    constexpr tag patient_name{0x0010, 0x0010};
    constexpr tag patient_id{0x0010, 0x0020};
    constexpr tag patient_birth_date{0x0010, 0x0030};
    constexpr tag patient_sex{0x0010, 0x0040};

    // Image Pixel Module
    constexpr tag rows{0x0028, 0x0010};
    constexpr tag columns{0x0028, 0x0011};
    constexpr tag bits_allocated{0x0028, 0x0100};
    constexpr tag bits_stored{0x0028, 0x0101};
    constexpr tag pixel_representation{0x0028, 0x0103};

    // Image Plane Module
    constexpr tag pixel_spacing{0x0028, 0x0030};
    constexpr tag image_position_patient{0x0020, 0x0032};
    constexpr tag image_orientation_patient{0x0020, 0x0037};
    constexpr tag slice_thickness{0x0018, 0x0050};

    // CT Image Module
    constexpr tag rescale_intercept{0x0028, 0x1052};
    constexpr tag rescale_slope{0x0028, 0x1053};
    constexpr tag window_center{0x0028, 0x1050};
    constexpr tag window_width{0x0028, 0x1051};

    // Pixel Data
    constexpr tag pixel_data{0x7FE0, 0x0010};
}

// 메타데이터 읽기 헬퍼
struct ImageMetadata {
    std::string patientName;
    std::string patientId;
    uint16_t rows;
    uint16_t columns;
    uint16_t bitsAllocated;
    uint16_t bitsStored;
    uint16_t pixelRepresentation;
    std::array<double, 2> pixelSpacing;
    std::array<double, 3> imagePosition;
    std::array<double, 6> imageOrientation;
    double sliceThickness;
    double rescaleIntercept;
    double rescaleSlope;

    static ImageMetadata fromDataset(const dicom_dataset& ds) {
        ImageMetadata meta;

        if (auto e = ds.find(dicom_tag::patient_name))
            meta.patientName = e->get_string();

        if (auto e = ds.find(dicom_tag::rows))
            meta.rows = e->get_uint16();

        if (auto e = ds.find(dicom_tag::columns))
            meta.columns = e->get_uint16();

        if (auto e = ds.find(dicom_tag::pixel_spacing)) {
            auto values = e->get_doubles();
            if (values.size() >= 2) {
                meta.pixelSpacing[0] = values[0];
                meta.pixelSpacing[1] = values[1];
            }
        }

        if (auto e = ds.find(dicom_tag::rescale_intercept))
            meta.rescaleIntercept = e->get_double();
        else
            meta.rescaleIntercept = 0.0;

        if (auto e = ds.find(dicom_tag::rescale_slope))
            meta.rescaleSlope = e->get_double();
        else
            meta.rescaleSlope = 1.0;

        return meta;
    }
};
```

### 2.4 픽셀 데이터 추출

```cpp
#include <pacs/core/dicom_file.hpp>
#include <pacs/encoding/transfer_syntax.hpp>

// 압축되지 않은 픽셀 데이터 추출
std::vector<int16_t> extractPixelData(const dicom_file& file) {
    const auto& dataset = file.dataset();

    auto rowsElem = dataset.find(dicom_tag::rows);
    auto colsElem = dataset.find(dicom_tag::columns);
    auto pixelElem = dataset.find(dicom_tag::pixel_data);

    if (!rowsElem || !colsElem || !pixelElem) {
        throw std::runtime_error("Missing required elements");
    }

    uint16_t rows = rowsElem->get_uint16();
    uint16_t cols = colsElem->get_uint16();
    size_t pixelCount = rows * cols;

    std::span<const std::byte> rawBytes = pixelElem->get_bytes();

    // int16_t로 변환 (CT 영상 가정)
    std::vector<int16_t> pixels(pixelCount);
    std::memcpy(pixels.data(), rawBytes.data(),
                std::min(rawBytes.size(), pixelCount * sizeof(int16_t)));

    return pixels;
}

// HU 변환 적용
std::vector<float> convertToHU(
    const std::vector<int16_t>& rawPixels,
    double rescaleSlope,
    double rescaleIntercept)
{
    std::vector<float> hu(rawPixels.size());
    for (size_t i = 0; i < rawPixels.size(); ++i) {
        hu[i] = static_cast<float>(
            rawPixels[i] * rescaleSlope + rescaleIntercept);
    }
    return hu;
}
```

---

## 3. pacs_system → ITK 변환

### 3.1 단일 슬라이스 변환

```cpp
#include <pacs/core/dicom_file.hpp>
#include <itkImage.h>
#include <itkImportImageFilter.h>

template<typename TPixel>
typename itk::Image<TPixel, 2>::Pointer
pacsToITK2D(const pacs::dicom_file& pacsFile)
{
    const auto& dataset = pacsFile.dataset();

    // 메타데이터 추출
    uint16_t rows = dataset.find(pacs::dicom_tag::rows)->get_uint16();
    uint16_t cols = dataset.find(pacs::dicom_tag::columns)->get_uint16();

    auto spacingValues = dataset.find(pacs::dicom_tag::pixel_spacing)->get_doubles();
    auto positionValues = dataset.find(pacs::dicom_tag::image_position_patient)->get_doubles();

    // 픽셀 데이터 추출
    auto pixelElement = dataset.find(pacs::dicom_tag::pixel_data);
    std::span<const std::byte> rawBytes = pixelElement->get_bytes();

    // ITK Import Filter 사용
    using ImageType = itk::Image<TPixel, 2>;
    using ImportFilterType = itk::ImportImageFilter<TPixel, 2>;

    auto importFilter = ImportFilterType::New();

    // Region 설정
    typename ImportFilterType::SizeType size;
    size[0] = cols;
    size[1] = rows;

    typename ImportFilterType::IndexType start;
    start.Fill(0);

    typename ImportFilterType::RegionType region;
    region.SetIndex(start);
    region.SetSize(size);
    importFilter->SetRegion(region);

    // Spacing 설정
    double spacing[2] = {spacingValues[1], spacingValues[0]};  // col, row 순서
    importFilter->SetSpacing(spacing);

    // Origin 설정
    double origin[2] = {positionValues[0], positionValues[1]};
    importFilter->SetOrigin(origin);

    // 픽셀 데이터 복사
    const size_t pixelCount = rows * cols;
    TPixel* pixelBuffer = new TPixel[pixelCount];
    std::memcpy(pixelBuffer, rawBytes.data(), pixelCount * sizeof(TPixel));

    importFilter->SetImportPointer(pixelBuffer, pixelCount, true);
    importFilter->Update();

    return importFilter->GetOutput();
}
```

### 3.2 시리즈 (3D 볼륨) 변환

```cpp
#include <pacs/core/dicom_file.hpp>
#include <itkImage.h>
#include <itkImportImageFilter.h>
#include <filesystem>
#include <algorithm>

// 슬라이스 정렬을 위한 구조체
struct SliceInfo {
    std::filesystem::path path;
    double sliceLocation;
    int instanceNumber;
};

template<typename TPixel>
typename itk::Image<TPixel, 3>::Pointer
pacsToITK3D(const std::filesystem::path& seriesDirectory)
{
    namespace fs = std::filesystem;

    // 1. 디렉토리에서 DICOM 파일 수집 및 정렬
    std::vector<SliceInfo> slices;

    for (const auto& entry : fs::directory_iterator(seriesDirectory)) {
        if (!entry.is_regular_file()) continue;

        auto result = pacs::dicom_file::open(entry.path().string());
        if (!result) continue;

        const auto& dataset = result.value().dataset();

        SliceInfo info;
        info.path = entry.path();

        if (auto e = dataset.find(pacs::dicom_tag::slice_location))
            info.sliceLocation = e->get_double();
        else if (auto e = dataset.find(pacs::dicom_tag::image_position_patient)) {
            auto pos = e->get_doubles();
            auto orient = dataset.find(pacs::dicom_tag::image_orientation_patient)->get_doubles();
            // 슬라이스 방향에 따른 위치 계산
            info.sliceLocation = pos[2];  // 간단히 Z 좌표 사용
        }

        if (auto e = dataset.find(pacs::dicom_tag::instance_number))
            info.instanceNumber = e->get_int32();

        slices.push_back(info);
    }

    // 슬라이스 위치로 정렬
    std::sort(slices.begin(), slices.end(),
        [](const SliceInfo& a, const SliceInfo& b) {
            return a.sliceLocation < b.sliceLocation;
        });

    if (slices.empty()) {
        throw std::runtime_error("No DICOM files found");
    }

    // 2. 첫 번째 슬라이스에서 공통 메타데이터 추출
    auto firstResult = pacs::dicom_file::open(slices[0].path.string());
    const auto& firstDataset = firstResult.value().dataset();

    uint16_t rows = firstDataset.find(pacs::dicom_tag::rows)->get_uint16();
    uint16_t cols = firstDataset.find(pacs::dicom_tag::columns)->get_uint16();
    size_t numSlices = slices.size();

    auto pixelSpacing = firstDataset.find(pacs::dicom_tag::pixel_spacing)->get_doubles();
    auto imagePosition = firstDataset.find(pacs::dicom_tag::image_position_patient)->get_doubles();

    // 슬라이스 간격 계산
    double sliceSpacing = 1.0;
    if (slices.size() > 1) {
        sliceSpacing = std::abs(slices[1].sliceLocation - slices[0].sliceLocation);
    } else if (auto e = firstDataset.find(pacs::dicom_tag::slice_thickness)) {
        sliceSpacing = e->get_double();
    }

    // 3. 3D 볼륨 버퍼 할당
    using ImageType = itk::Image<TPixel, 3>;
    using ImportFilterType = itk::ImportImageFilter<TPixel, 3>;

    const size_t sliceSize = rows * cols;
    const size_t totalPixels = sliceSize * numSlices;
    TPixel* volumeBuffer = new TPixel[totalPixels];

    // 4. 각 슬라이스 데이터 복사
    for (size_t i = 0; i < slices.size(); ++i) {
        auto sliceResult = pacs::dicom_file::open(slices[i].path.string());
        const auto& sliceDataset = sliceResult.value().dataset();

        auto pixelElement = sliceDataset.find(pacs::dicom_tag::pixel_data);
        std::span<const std::byte> rawBytes = pixelElement->get_bytes();

        std::memcpy(volumeBuffer + i * sliceSize,
                    rawBytes.data(),
                    sliceSize * sizeof(TPixel));
    }

    // 5. ITK Import Filter 설정
    auto importFilter = ImportFilterType::New();

    typename ImportFilterType::SizeType size;
    size[0] = cols;
    size[1] = rows;
    size[2] = numSlices;

    typename ImportFilterType::IndexType start;
    start.Fill(0);

    typename ImportFilterType::RegionType region;
    region.SetIndex(start);
    region.SetSize(size);
    importFilter->SetRegion(region);

    // Spacing (VTK/ITK는 LPS 좌표계 사용)
    double spacing[3] = {pixelSpacing[1], pixelSpacing[0], sliceSpacing};
    importFilter->SetSpacing(spacing);

    // Origin
    double origin[3] = {imagePosition[0], imagePosition[1], imagePosition[2]};
    importFilter->SetOrigin(origin);

    // Direction Matrix (Image Orientation에서 추출)
    if (auto orientElem = firstDataset.find(pacs::dicom_tag::image_orientation_patient)) {
        auto orient = orientElem->get_doubles();
        if (orient.size() >= 6) {
            typename ImageType::DirectionType direction;

            // Row direction (첫 3개)
            direction[0][0] = orient[0];
            direction[1][0] = orient[1];
            direction[2][0] = orient[2];

            // Column direction (다음 3개)
            direction[0][1] = orient[3];
            direction[1][1] = orient[4];
            direction[2][1] = orient[5];

            // Slice direction (외적)
            direction[0][2] = orient[1]*orient[5] - orient[2]*orient[4];
            direction[1][2] = orient[2]*orient[3] - orient[0]*orient[5];
            direction[2][2] = orient[0]*orient[4] - orient[1]*orient[3];

            importFilter->SetDirection(direction);
        }
    }

    importFilter->SetImportPointer(volumeBuffer, totalPixels, true);
    importFilter->Update();

    return importFilter->GetOutput();
}
```

---

## 4. pacs_system → VTK 변환

### 4.1 직접 변환 (ITK 없이)

```cpp
#include <pacs/core/dicom_file.hpp>
#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <vtkImageImport.h>

vtkSmartPointer<vtkImageData> pacsToVTK3D(
    const std::filesystem::path& seriesDirectory)
{
    // pacs_system으로 DICOM 시리즈 읽기 (위와 동일한 로직)
    // ... 슬라이스 수집 및 정렬 ...

    // VTK Image Import
    auto imageImport = vtkSmartPointer<vtkImageImport>::New();

    // 데이터 타입 설정
    imageImport->SetDataScalarTypeToShort();  // CT용 int16
    imageImport->SetNumberOfScalarComponents(1);

    // 차원 설정
    imageImport->SetWholeExtent(0, cols - 1, 0, rows - 1, 0, numSlices - 1);
    imageImport->SetDataExtent(0, cols - 1, 0, rows - 1, 0, numSlices - 1);

    // Spacing
    imageImport->SetDataSpacing(pixelSpacing[1], pixelSpacing[0], sliceSpacing);

    // Origin
    imageImport->SetDataOrigin(imagePosition[0], imagePosition[1], imagePosition[2]);

    // 데이터 포인터 설정
    imageImport->SetImportVoidPointer(volumeBuffer);
    imageImport->Update();

    // DeepCopy로 독립 복사본 생성
    auto imageData = vtkSmartPointer<vtkImageData>::New();
    imageData->DeepCopy(imageImport->GetOutput());

    delete[] volumeBuffer;

    return imageData;
}
```

### 4.2 통합 어댑터 클래스

```cpp
#include <pacs/core/dicom_file.hpp>
#include <itkImage.h>
#include <itkImageToVTKImageFilter.h>
#include <vtkSmartPointer.h>
#include <vtkImageData.h>

class PACSToViewerAdapter
{
public:
    using PixelType = int16_t;
    using ITKImageType = itk::Image<PixelType, 3>;
    using ITKToVTKType = itk::ImageToVTKImageFilter<ITKImageType>;

private:
    ITKImageType::Pointer m_itkImage;
    ITKToVTKType::Pointer m_connector;
    vtkSmartPointer<vtkImageData> m_vtkImage;

    struct Metadata {
        std::string patientName;
        std::string patientId;
        std::string studyDate;
        std::string seriesDescription;
        std::string modality;
        double windowCenter;
        double windowWidth;
    };
    Metadata m_metadata;

public:
    bool LoadSeries(const std::filesystem::path& directory)
    {
        try {
            // pacs_system으로 읽기
            m_itkImage = pacsToITK3D<PixelType>(directory);

            // 메타데이터 추출 (첫 파일에서)
            ExtractMetadata(directory);

            // ITK → VTK 변환
            m_connector = ITKToVTKType::New();
            m_connector->SetInput(m_itkImage);
            m_connector->Update();

            m_vtkImage = vtkSmartPointer<vtkImageData>::New();
            m_vtkImage->DeepCopy(m_connector->GetOutput());

            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "Error loading series: " << e.what() << std::endl;
            return false;
        }
    }

    // ITK 이미지 접근 (필터링용)
    ITKImageType::Pointer GetITKImage() { return m_itkImage; }

    // VTK 이미지 접근 (시각화용)
    vtkImageData* GetVTKImage() { return m_vtkImage; }

    // 메타데이터 접근
    const Metadata& GetMetadata() const { return m_metadata; }

    // 기본 Window/Level 획득
    std::pair<double, double> GetDefaultWindowLevel() const {
        return {m_metadata.windowWidth, m_metadata.windowCenter};
    }

private:
    void ExtractMetadata(const std::filesystem::path& directory)
    {
        namespace fs = std::filesystem;

        for (const auto& entry : fs::directory_iterator(directory)) {
            if (!entry.is_regular_file()) continue;

            auto result = pacs::dicom_file::open(entry.path().string());
            if (!result) continue;

            const auto& ds = result.value().dataset();

            if (auto e = ds.find(pacs::dicom_tag::patient_name))
                m_metadata.patientName = e->get_string();
            if (auto e = ds.find(pacs::dicom_tag::patient_id))
                m_metadata.patientId = e->get_string();
            if (auto e = ds.find(pacs::dicom_tag::modality))
                m_metadata.modality = e->get_string();
            if (auto e = ds.find(pacs::dicom_tag::window_center))
                m_metadata.windowCenter = e->get_double();
            if (auto e = ds.find(pacs::dicom_tag::window_width))
                m_metadata.windowWidth = e->get_double();

            break;  // 첫 파일만 읽기
        }
    }
};
```

---

## 5. 네트워크 서비스 연동

### 5.1 C-FIND (Query)

```cpp
#include <pacs/services/query_scu.hpp>
#include <pacs/network/association.hpp>

// PACS 서버에서 환자/연구 검색
std::vector<pacs::dicom_dataset> queryStudies(
    const std::string& pacsHost,
    uint16_t pacsPort,
    const std::string& patientName)
{
    pacs::query_scu scu;

    // Association 설정
    pacs::association_config config;
    config.calling_ae_title = "DICOM_VIEWER";
    config.called_ae_title = "PACS_SERVER";
    config.host = pacsHost;
    config.port = pacsPort;

    // 쿼리 조건 설정
    pacs::dicom_dataset query;
    query.set_string(pacs::dicom_tag::patient_name, patientName + "*");
    query.set_string(pacs::dicom_tag::query_retrieve_level, "STUDY");

    // 반환 받을 필드 지정
    query.set_empty(pacs::dicom_tag::study_instance_uid);
    query.set_empty(pacs::dicom_tag::study_date);
    query.set_empty(pacs::dicom_tag::study_description);
    query.set_empty(pacs::dicom_tag::modalities_in_study);

    // 쿼리 실행
    auto result = scu.find(config, query, pacs::query_level::study);
    if (!result) {
        throw std::runtime_error("Query failed: " + result.error().message());
    }

    return result.value();
}
```

### 5.2 C-MOVE (Retrieve)

```cpp
#include <pacs/services/retrieve_scu.hpp>

// PACS에서 영상 가져오기
void retrieveStudy(
    const std::string& pacsHost,
    uint16_t pacsPort,
    const std::string& studyInstanceUid,
    const std::string& destinationAE)
{
    pacs::retrieve_scu scu;

    pacs::association_config config;
    config.calling_ae_title = "DICOM_VIEWER";
    config.called_ae_title = "PACS_SERVER";
    config.host = pacsHost;
    config.port = pacsPort;

    pacs::dicom_dataset request;
    request.set_string(pacs::dicom_tag::study_instance_uid, studyInstanceUid);
    request.set_string(pacs::dicom_tag::query_retrieve_level, "STUDY");

    auto result = scu.move(config, request, destinationAE);
    if (!result) {
        throw std::runtime_error("Retrieve failed: " + result.error().message());
    }
}
```

### 5.3 C-STORE SCP (수신)

```cpp
#include <pacs/services/storage_scp.hpp>

// 영상 수신 서버 실행
void runStorageServer(
    uint16_t port,
    const std::string& storageDirectory,
    std::function<void(const pacs::dicom_file&)> onImageReceived)
{
    pacs::storage_scp_config config;
    config.ae_title = "DICOM_VIEWER";
    config.port = port;
    config.storage_path = storageDirectory;

    // 콜백 설정
    config.on_store_completed = [&](const pacs::dicom_file& file) {
        onImageReceived(file);
    };

    pacs::storage_scp server(config);
    server.start();

    // 서버 실행 (별도 스레드에서)
    // server.run();
}
```

---

## 6. 압축 코덱 처리

### 6.1 Transfer Syntax 지원

pacs_system은 다양한 Transfer Syntax를 지원합니다:

| Transfer Syntax UID | 이름 | 지원 |
|---------------------|------|------|
| 1.2.840.10008.1.2 | Implicit VR Little Endian | ✅ |
| 1.2.840.10008.1.2.1 | Explicit VR Little Endian | ✅ |
| 1.2.840.10008.1.2.2 | Explicit VR Big Endian | ✅ |
| 1.2.840.10008.1.2.4.50 | JPEG Baseline | ✅ |
| 1.2.840.10008.1.2.4.70 | JPEG Lossless | ✅ |
| 1.2.840.10008.1.2.4.90 | JPEG 2000 Lossless | ✅ |
| 1.2.840.10008.1.2.4.91 | JPEG 2000 | ✅ |
| 1.2.840.10008.1.2.4.80 | JPEG-LS Lossless | ✅ |
| 1.2.840.10008.1.2.5 | RLE Lossless | ✅ |

### 6.2 압축 해제

```cpp
#include <pacs/encoding/transfer_syntax.hpp>
#include <pacs/encoding/codec_factory.hpp>

std::vector<std::byte> decompressPixelData(
    const pacs::dicom_file& file)
{
    const auto& meta = file.file_meta_information();
    const auto& dataset = file.dataset();

    // Transfer Syntax 확인
    auto tsElement = meta.find(pacs::dicom_tag::transfer_syntax_uid);
    std::string transferSyntax = tsElement->get_string();

    // 픽셀 데이터 추출
    auto pixelElement = dataset.find(pacs::dicom_tag::pixel_data);
    auto compressedData = pixelElement->get_bytes();

    // 압축 해제가 필요한 경우
    if (pacs::transfer_syntax::is_compressed(transferSyntax)) {
        auto codec = pacs::codec_factory::create(transferSyntax);

        // 이미지 파라미터 추출
        pacs::image_params params;
        params.rows = dataset.find(pacs::dicom_tag::rows)->get_uint16();
        params.columns = dataset.find(pacs::dicom_tag::columns)->get_uint16();
        params.bits_allocated = dataset.find(pacs::dicom_tag::bits_allocated)->get_uint16();
        params.bits_stored = dataset.find(pacs::dicom_tag::bits_stored)->get_uint16();
        params.pixel_representation = dataset.find(pacs::dicom_tag::pixel_representation)->get_uint16();

        return codec->decode(compressedData, params);
    }

    // 압축되지 않은 경우 직접 복사
    return std::vector<std::byte>(compressedData.begin(), compressedData.end());
}
```

---

## 7. CMake 빌드 설정

### 7.1 프로젝트 설정

```cmake
cmake_minimum_required(VERSION 3.20)
project(dicom_viewer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# pacs_system 찾기
find_package(pacs_system REQUIRED COMPONENTS
    pacs_core
    pacs_encoding
    pacs_network
    pacs_services
)

# ITK 찾기
find_package(ITK REQUIRED COMPONENTS
    ITKCommon
    ITKIOImageBase
    ITKVtkGlue
)
include(${ITK_USE_FILE})

# VTK 찾기
find_package(VTK REQUIRED COMPONENTS
    CommonCore
    CommonDataModel
    IOImage
    RenderingCore
    RenderingOpenGL2
    RenderingVolumeOpenGL2
    InteractionStyle
    GUISupportQt
)

# Qt 찾기
find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets)

add_executable(dicom_viewer
    src/main.cpp
    src/PACSToViewerAdapter.cpp
    src/MainWindow.cpp
    src/VolumeRenderer.cpp
)

target_link_libraries(dicom_viewer PRIVATE
    pacs::pacs_core
    pacs::pacs_encoding
    pacs::pacs_services
    ${ITK_LIBRARIES}
    ${VTK_LIBRARIES}
    Qt6::Widgets
    Qt6::OpenGLWidgets
)

vtk_module_autoinit(
    TARGETS dicom_viewer
    MODULES ${VTK_LIBRARIES}
)
```

---

## 8. 통합 Viewer 구조 예시

```
dicom_viewer/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── PACSToViewerAdapter.hpp
│   │   ├── PACSToViewerAdapter.cpp
│   │   ├── ImagePipeline.hpp
│   │   └── ImagePipeline.cpp
│   ├── processing/
│   │   ├── Preprocessor.hpp         # ITK 전처리
│   │   ├── Segmentor.hpp            # ITK 분할
│   │   └── SurfaceExtractor.hpp     # VTK 표면 추출
│   ├── visualization/
│   │   ├── VolumeRenderer.hpp       # VTK 볼륨 렌더링
│   │   ├── SliceViewer.hpp          # VTK MPR
│   │   └── SurfaceRenderer.hpp      # VTK 표면 렌더링
│   ├── network/
│   │   ├── QueryClient.hpp          # pacs_system C-FIND
│   │   ├── RetrieveClient.hpp       # pacs_system C-MOVE
│   │   └── StorageServer.hpp        # pacs_system C-STORE SCP
│   └── ui/
│       ├── MainWindow.hpp
│       ├── MainWindow.cpp
│       └── *.ui
├── include/
│   └── dicom_viewer/
│       └── *.hpp
└── docs/
    └── reference/
        └── *.md
```

---

## 9. 참고 자료

### pacs_system 문서
- `/Users/dongcheolshin/Sources/pacs_system/docs/ARCHITECTURE.md`
- `/Users/dongcheolshin/Sources/pacs_system/docs/API_REFERENCE.md`

### 예제 코드
- `/Users/dongcheolshin/Sources/pacs_system/examples/dcm_dump/` - DICOM 파일 덤프
- `/Users/dongcheolshin/Sources/pacs_system/examples/dcm_info/` - DICOM 정보 추출
- `/Users/dongcheolshin/Sources/pacs_system/examples/store_scp/` - Storage SCP 예제

---

*이전 문서: [04-dicom-pipeline.md](04-dicom-pipeline.md) - DICOM 처리 파이프라인*
*다음 문서: [README.md](README.md) - 문서 인덱스*

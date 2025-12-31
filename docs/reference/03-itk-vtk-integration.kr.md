# ITK-VTK 통합 가이드

> **Last Updated**: 2025-12-31
> **Reference**: [ITK VtkGlue Module](https://github.com/InsightSoftwareConsortium/ITK/tree/master/Modules/Bridge/VtkGlue)

## 1. 개요

### 1.1 ITK와 VTK의 역할 분담

ITK와 VTK는 각각 고유한 강점을 가지며, 의료 영상 처리에서 상호 보완적으로 사용됩니다:

```
┌─────────────────────────────────────────────────────────────────┐
│              ITK + VTK Integration Architecture                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   ┌─────────────── ITK ───────────────┐                        │
│   │                                   │                        │
│   │  • DICOM 읽기/쓰기                │                        │
│   │  • 영상 필터링 (Smoothing, Edge)  │                        │
│   │  • 영상 분할 (Segmentation)       │                        │
│   │  • 영상 정합 (Registration)       │                        │
│   │  • 공간 좌표 변환                 │                        │
│   │                                   │                        │
│   │         itk::Image<T, D>          │                        │
│   └───────────────┬───────────────────┘                        │
│                   │                                             │
│                   ↓ ITKVtkGlue                                  │
│                   │                                             │
│   ┌───────────────┴───────────────────┐                        │
│   │                                   │                        │
│   │  • 2D/3D 시각화                   │                        │
│   │  • 볼륨 렌더링                    │                        │
│   │  • 표면 렌더링                    │                        │
│   │  • 상호작용 위젯                  │                        │
│   │  • GUI 통합 (Qt)                  │                        │
│   │                                   │                        │
│   │         vtkImageData              │                        │
│   └─────────────── VTK ───────────────┘                        │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 1.2 왜 통합이 필요한가?

| 기능 | ITK | VTK |
|------|-----|-----|
| **DICOM 읽기** | ✅ 완전 지원 (GDCM) | ⚠️ 제한적 |
| **영상 분할** | ✅ 다양한 알고리즘 | ❌ 기본만 |
| **영상 정합** | ✅ 완전 프레임워크 | ❌ 없음 |
| **필터링** | ✅ 의료 특화 | ✅ 일반 목적 |
| **3D 렌더링** | ❌ 없음 | ✅ 완전 지원 |
| **볼륨 렌더링** | ❌ 없음 | ✅ GPU 가속 |
| **상호작용** | ❌ 없음 | ✅ 위젯 라이브러리 |
| **GUI 통합** | ❌ 없음 | ✅ Qt/Tk |

---

## 2. 데이터 변환

### 2.1 ITKVtkGlue 모듈

ITK의 `ITKVtkGlue` 모듈은 ITK와 VTK 간의 데이터 변환을 담당합니다:

```cpp
// 필요한 헤더
#include <itkImageToVTKImageFilter.h>
#include <itkVTKImageToImageFilter.h>
```

### 2.2 ITK → VTK 변환

```cpp
#include <itkImage.h>
#include <itkImageToVTKImageFilter.h>
#include <vtkImageData.h>

using ImageType = itk::Image<short, 3>;
using ConnectorType = itk::ImageToVTKImageFilter<ImageType>;

// ITK 이미지 읽기
auto reader = itk::ImageFileReader<ImageType>::New();
reader->SetFileName("ct_image.dcm");
reader->Update();

// ITK → VTK 변환
auto connector = ConnectorType::New();
connector->SetInput(reader->GetOutput());
connector->Update();

// VTK 이미지 데이터 획득
vtkImageData* vtkImage = connector->GetOutput();

// 주의: connector가 스코프를 벗어나면 vtkImage도 무효화됨
// 필요시 DeepCopy 사용:
auto safeImage = vtkSmartPointer<vtkImageData>::New();
safeImage->DeepCopy(connector->GetOutput());
```

### 2.3 VTK → ITK 변환

```cpp
#include <itkVTKImageToImageFilter.h>

using ImageType = itk::Image<short, 3>;
using ConnectorType = itk::VTKImageToImageFilter<ImageType>;

// VTK 이미지에서 ITK 이미지로 변환
auto connector = ConnectorType::New();
connector->SetInput(vtkImageData);
connector->Update();

// ITK 이미지 획득
ImageType::Pointer itkImage = connector->GetOutput();

// 파이프라인에서 분리 (선택적)
itkImage->DisconnectPipeline();
```

### 2.4 데이터 타입 매핑

| ITK 타입 | VTK 타입 | 용도 |
|----------|----------|------|
| `itk::Image<short, 3>` | `VTK_SHORT` | CT 영상 (HU) |
| `itk::Image<unsigned short, 3>` | `VTK_UNSIGNED_SHORT` | 일반 의료 영상 |
| `itk::Image<float, 3>` | `VTK_FLOAT` | 정밀 계산, MRI |
| `itk::Image<unsigned char, 3>` | `VTK_UNSIGNED_CHAR` | 마스크, 레이블 |
| `itk::Image<itk::RGBPixel<unsigned char>, 3>` | `VTK_UNSIGNED_CHAR` (3 comp) | 컬러 영상 |

---

## 3. 좌표계 통합

### 3.1 좌표계 차이점

ITK와 VTK 모두 **LPS (Left-Posterior-Superior)** 좌표계를 사용하지만, 내부 표현 방식에 차이가 있습니다:

```
┌─────────────────────────────────────────────────────────────────┐
│                    Coordinate System Comparison                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│    ITK Image                         VTK ImageData              │
│    ─────────                         ──────────────             │
│                                                                 │
│    • Origin: 첫 번째 픽셀 중심       • Origin: 첫 번째 픽셀 중심│
│    • Direction Matrix: 3x3           • Direction: 지원 안함 *   │
│    • Spacing: mm 단위               • Spacing: mm 단위         │
│                                                                 │
│    Physical Point = Origin           Physical Point = Origin    │
│                    + Direction        + Index * Spacing         │
│                    * Index                                      │
│                    * Spacing                                    │
│                                                                 │
│    * VTK 9.0+에서 Direction 지원 추가                           │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 Direction Matrix 처리

```cpp
// ITK 이미지의 Direction Matrix 획득
auto direction = itkImage->GetDirection();

// VTK에서 Direction 적용 (VTK 9.0+)
#if VTK_MAJOR_VERSION >= 9
    double directionMatrix[9];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            directionMatrix[i * 3 + j] = direction[i][j];
        }
    }
    vtkImage->SetDirectionMatrix(directionMatrix);
#else
    // VTK 8.x 이하: vtkTransform 또는 vtkMatrix4x4 사용
    auto transform = vtkSmartPointer<vtkTransform>::New();
    // ... Direction 적용 로직
#endif
```

### 3.3 좌표 변환 유틸리티

```cpp
// Physical Point → Index 변환
template<typename ImageType>
typename ImageType::IndexType
PhysicalToIndex(typename ImageType::Pointer image,
                typename ImageType::PointType point)
{
    typename ImageType::IndexType index;
    image->TransformPhysicalPointToIndex(point, index);
    return index;
}

// Index → Physical Point 변환
template<typename ImageType>
typename ImageType::PointType
IndexToPhysical(typename ImageType::Pointer image,
                typename ImageType::IndexType index)
{
    typename ImageType::PointType point;
    image->TransformIndexToPhysicalPoint(index, point);
    return point;
}
```

---

## 4. 통합 파이프라인 구현

### 4.1 기본 통합 파이프라인

```cpp
#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkGDCMImageIO.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkImageSeriesReader.h>
#include <itkDiscreteGaussianImageFilter.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkImageToVTKImageFilter.h>

#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkVolume.h>
#include <vtkVolumeProperty.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>

class IntegratedPipeline
{
public:
    using PixelType = short;
    static constexpr unsigned int Dimension = 3;
    using ImageType = itk::Image<PixelType, Dimension>;

    // DICOM 시리즈 읽기 (ITK)
    ImageType::Pointer ReadDICOMSeries(const std::string& directory)
    {
        using NamesGeneratorType = itk::GDCMSeriesFileNames;
        auto namesGenerator = NamesGeneratorType::New();
        namesGenerator->SetDirectory(directory);

        auto seriesUID = namesGenerator->GetSeriesUIDs();
        if (seriesUID.empty()) {
            throw std::runtime_error("No DICOM series found");
        }

        auto fileNames = namesGenerator->GetFileNames(seriesUID.front());

        using ReaderType = itk::ImageSeriesReader<ImageType>;
        auto reader = ReaderType::New();
        reader->SetFileNames(fileNames);

        auto dicomIO = itk::GDCMImageIO::New();
        reader->SetImageIO(dicomIO);
        reader->Update();

        return reader->GetOutput();
    }

    // ITK 필터링
    ImageType::Pointer ApplyGaussianSmoothing(
        ImageType::Pointer input, double sigma)
    {
        using FilterType = itk::DiscreteGaussianImageFilter<ImageType, ImageType>;
        auto filter = FilterType::New();
        filter->SetInput(input);
        filter->SetVariance(sigma * sigma);
        filter->Update();
        return filter->GetOutput();
    }

    // ITK 분할
    using MaskImageType = itk::Image<unsigned char, Dimension>;
    MaskImageType::Pointer ThresholdSegmentation(
        ImageType::Pointer input,
        PixelType lower, PixelType upper)
    {
        using FilterType = itk::BinaryThresholdImageFilter<ImageType, MaskImageType>;
        auto filter = FilterType::New();
        filter->SetInput(input);
        filter->SetLowerThreshold(lower);
        filter->SetUpperThreshold(upper);
        filter->SetInsideValue(255);
        filter->SetOutsideValue(0);
        filter->Update();
        return filter->GetOutput();
    }

    // ITK → VTK 변환 및 렌더링
    void RenderVolume(ImageType::Pointer itkImage)
    {
        // ITK → VTK 변환
        using ConnectorType = itk::ImageToVTKImageFilter<ImageType>;
        auto connector = ConnectorType::New();
        connector->SetInput(itkImage);
        connector->Update();

        // VTK 볼륨 렌더링 설정
        auto volumeMapper = vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
        volumeMapper->SetInputData(connector->GetOutput());

        auto colorFunc = vtkSmartPointer<vtkColorTransferFunction>::New();
        colorFunc->AddRGBPoint(-1000, 0.0, 0.0, 0.0);
        colorFunc->AddRGBPoint(400, 1.0, 1.0, 0.9);

        auto opacityFunc = vtkSmartPointer<vtkPiecewiseFunction>::New();
        opacityFunc->AddPoint(-1000, 0.0);
        opacityFunc->AddPoint(400, 0.85);

        auto volumeProperty = vtkSmartPointer<vtkVolumeProperty>::New();
        volumeProperty->SetColor(colorFunc);
        volumeProperty->SetScalarOpacity(opacityFunc);
        volumeProperty->ShadeOn();

        auto volume = vtkSmartPointer<vtkVolume>::New();
        volume->SetMapper(volumeMapper);
        volume->SetProperty(volumeProperty);

        // 렌더링
        auto renderer = vtkSmartPointer<vtkRenderer>::New();
        renderer->AddVolume(volume);
        renderer->SetBackground(0.1, 0.1, 0.2);

        auto renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
        renderWindow->AddRenderer(renderer);
        renderWindow->SetSize(800, 600);

        auto interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
        interactor->SetRenderWindow(renderWindow);

        renderWindow->Render();
        interactor->Start();
    }
};
```

### 4.2 사용 예시

```cpp
int main(int argc, char* argv[])
{
    IntegratedPipeline pipeline;

    // 1. DICOM 읽기 (ITK)
    auto image = pipeline.ReadDICOMSeries("/path/to/dicom");

    // 2. 전처리 (ITK)
    auto smoothed = pipeline.ApplyGaussianSmoothing(image, 1.0);

    // 3. 분할 (ITK)
    auto boneMask = pipeline.ThresholdSegmentation(smoothed, 200, 3000);

    // 4. 시각화 (VTK)
    pipeline.RenderVolume(smoothed);

    return 0;
}
```

---

## 5. 메시 변환

### 5.1 ITK Mesh → VTK PolyData

```cpp
#include <itkMesh.h>
#include <itkMeshFileReader.h>
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>

template<typename TMesh>
vtkSmartPointer<vtkPolyData> ITKMeshToVTKPolyData(typename TMesh::Pointer mesh)
{
    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    auto points = vtkSmartPointer<vtkPoints>::New();
    auto cells = vtkSmartPointer<vtkCellArray>::New();

    // 점 복사
    for (auto it = mesh->GetPoints()->Begin();
         it != mesh->GetPoints()->End(); ++it)
    {
        auto point = it->Value();
        points->InsertNextPoint(point[0], point[1], point[2]);
    }

    // 셀 복사 (삼각형)
    for (auto it = mesh->GetCells()->Begin();
         it != mesh->GetCells()->End(); ++it)
    {
        auto cell = it->Value();
        if (cell->GetNumberOfPoints() == 3)
        {
            vtkIdType pts[3];
            auto pointIt = cell->PointIdsBegin();
            for (int i = 0; i < 3; ++i, ++pointIt)
            {
                pts[i] = *pointIt;
            }
            cells->InsertNextCell(3, pts);
        }
    }

    polyData->SetPoints(points);
    polyData->SetPolys(cells);

    return polyData;
}
```

### 5.2 VTK PolyData → ITK Mesh

```cpp
template<typename TMesh>
typename TMesh::Pointer VTKPolyDataToITKMesh(vtkPolyData* polyData)
{
    auto mesh = TMesh::New();

    // 점 복사
    for (vtkIdType i = 0; i < polyData->GetNumberOfPoints(); ++i)
    {
        double pt[3];
        polyData->GetPoint(i, pt);
        typename TMesh::PointType point;
        point[0] = pt[0];
        point[1] = pt[1];
        point[2] = pt[2];
        mesh->SetPoint(i, point);
    }

    // 셀 복사
    vtkIdType cellId = 0;
    for (vtkIdType i = 0; i < polyData->GetNumberOfPolys(); ++i)
    {
        vtkCell* cell = polyData->GetCell(i);
        typename TMesh::CellAutoPointer cellPointer;

        using CellType = itk::TriangleCell<typename TMesh::CellType>;
        auto newCell = new CellType;

        for (vtkIdType j = 0; j < cell->GetNumberOfPoints(); ++j)
        {
            newCell->SetPointId(j, cell->GetPointId(j));
        }

        cellPointer.TakeOwnership(newCell);
        mesh->SetCell(cellId++, cellPointer);
    }

    return mesh;
}
```

---

## 6. 고급 통합 패턴

### 6.1 양방향 업데이트 패턴

```cpp
class BidirectionalImageBridge
{
public:
    using ITKImageType = itk::Image<short, 3>;
    using ITKToVTKType = itk::ImageToVTKImageFilter<ITKImageType>;
    using VTKToITKType = itk::VTKImageToImageFilter<ITKImageType>;

private:
    ITKImageType::Pointer m_itkImage;
    vtkSmartPointer<vtkImageData> m_vtkImage;
    ITKToVTKType::Pointer m_itkToVtk;
    VTKToITKType::Pointer m_vtkToItk;

    bool m_itkModified = false;
    bool m_vtkModified = false;

public:
    void SetITKImage(ITKImageType::Pointer image)
    {
        m_itkImage = image;
        m_itkModified = true;
    }

    void SetVTKImage(vtkImageData* image)
    {
        if (!m_vtkImage) {
            m_vtkImage = vtkSmartPointer<vtkImageData>::New();
        }
        m_vtkImage->DeepCopy(image);
        m_vtkModified = true;
    }

    vtkImageData* GetVTKImage()
    {
        if (m_itkModified) {
            SyncITKToVTK();
            m_itkModified = false;
        }
        return m_vtkImage;
    }

    ITKImageType::Pointer GetITKImage()
    {
        if (m_vtkModified) {
            SyncVTKToITK();
            m_vtkModified = false;
        }
        return m_itkImage;
    }

private:
    void SyncITKToVTK()
    {
        if (!m_itkToVtk) {
            m_itkToVtk = ITKToVTKType::New();
        }
        m_itkToVtk->SetInput(m_itkImage);
        m_itkToVtk->Update();

        if (!m_vtkImage) {
            m_vtkImage = vtkSmartPointer<vtkImageData>::New();
        }
        m_vtkImage->DeepCopy(m_itkToVtk->GetOutput());
    }

    void SyncVTKToITK()
    {
        if (!m_vtkToItk) {
            m_vtkToItk = VTKToITKType::New();
        }
        m_vtkToItk->SetInput(m_vtkImage);
        m_vtkToItk->Update();

        m_itkImage = m_vtkToItk->GetOutput();
        m_itkImage->DisconnectPipeline();
    }
};
```

### 6.2 파이프라인 캐싱

```cpp
template<typename TInputImage, typename TOutputImage>
class CachedFilter
{
public:
    using InputImageType = TInputImage;
    using OutputImageType = TOutputImage;
    using FilterType = itk::ImageToImageFilter<InputImageType, OutputImageType>;

private:
    typename FilterType::Pointer m_filter;
    typename OutputImageType::Pointer m_cachedOutput;
    itk::ModifiedTimeType m_lastModifiedTime = 0;

public:
    void SetFilter(typename FilterType::Pointer filter)
    {
        m_filter = filter;
    }

    typename OutputImageType::Pointer GetOutput()
    {
        auto currentTime = m_filter->GetInput()->GetMTime();
        if (currentTime > m_lastModifiedTime || !m_cachedOutput) {
            m_filter->Update();
            m_cachedOutput = m_filter->GetOutput();
            m_cachedOutput->DisconnectPipeline();
            m_lastModifiedTime = currentTime;
        }
        return m_cachedOutput;
    }
};
```

---

## 7. CMake 빌드 설정

### 7.1 통합 빌드 설정

```cmake
cmake_minimum_required(VERSION 3.16)
project(ITKVTKIntegration)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ITK 찾기
find_package(ITK REQUIRED COMPONENTS
    ITKCommon
    ITKIOImageBase
    ITKIOGDCM
    ITKImageFilterBase
    ITKSmoothing
    ITKThresholding
    ITKVtkGlue          # ITK-VTK 연결 모듈
)
include(${ITK_USE_FILE})

# VTK 찾기
find_package(VTK REQUIRED COMPONENTS
    CommonCore
    CommonDataModel
    CommonExecutionModel
    IOImage
    ImagingCore
    InteractionStyle
    RenderingCore
    RenderingOpenGL2
    RenderingVolumeOpenGL2
)

# 실행 파일
add_executable(itk_vtk_viewer
    main.cpp
    IntegratedPipeline.cpp
)

target_link_libraries(itk_vtk_viewer PRIVATE
    ${ITK_LIBRARIES}
    ${VTK_LIBRARIES}
)

# VTK 자동 초기화
vtk_module_autoinit(
    TARGETS itk_vtk_viewer
    MODULES ${VTK_LIBRARIES}
)
```

### 7.2 vcpkg 설정 (선택)

```json
{
    "dependencies": [
        {
            "name": "itk",
            "features": ["vtk"]
        },
        {
            "name": "vtk",
            "features": ["qt"]
        }
    ]
}
```

---

## 8. 성능 고려사항

### 8.1 메모리 관리

```cpp
// 권장: 불필요한 복사 피하기
auto connector = ITKToVTKType::New();
connector->SetInput(itkImage);
// GetOutput()을 직접 사용 - 복사 없음
volumeMapper->SetInputData(connector->GetOutput());

// 주의: connector 생명주기 관리 필요
// connector가 파괴되면 GetOutput()도 무효화됨

// 필요시 DeepCopy로 독립 복사본 생성
auto safeCopy = vtkSmartPointer<vtkImageData>::New();
safeCopy->DeepCopy(connector->GetOutput());
```

### 8.2 파이프라인 최적화

```cpp
// 스트리밍 처리 (대용량 데이터)
auto reader = itk::ImageFileReader<ImageType>::New();
reader->SetFileName("large_image.nrrd");
reader->UseStreamingOn();

// 멀티스레딩 활성화
itk::MultiThreaderBase::SetGlobalDefaultNumberOfThreads(
    std::thread::hardware_concurrency()
);

// VTK GPU 렌더링 확인
auto volumeMapper = vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
if (!volumeMapper->IsRenderSupported(renderWindow, volumeProperty)) {
    // CPU 폴백
    auto cpuMapper = vtkSmartPointer<vtkFixedPointVolumeRayCastMapper>::New();
    // ...
}
```

---

## 9. 참고 자료

### 공식 문서
- [ITK VtkGlue Documentation](https://itk.org/ITKSoftwareGuide/html/Book1/ITKSoftwareGuide-Book1ch3.html)
- [VTK Examples with ITK](https://examples.vtk.org/)

### 관련 프로젝트
- [MITK](https://www.mitk.org/) - ITK+VTK 통합 프레임워크
- [3D Slicer](https://www.slicer.org/) - 의료 영상 플랫폼
- [ITK-SNAP](http://www.itksnap.org/) - 분할 도구

---

*이전 문서: [02-vtk-overview.md](02-vtk-overview.md) - VTK 개요*
*다음 문서: [04-dicom-pipeline.md](04-dicom-pipeline.md) - DICOM 의료영상 처리 파이프라인*

# ITK (Insight Toolkit) 개요 및 아키텍처

> **Version**: ITK 5.x
> **Last Updated**: 2025-12-31
> **Reference**: [ITK Official Site](https://itk.org/), [ITK GitHub](https://github.com/InsightSoftwareConsortium/ITK)

## 1. ITK 소개

### 1.1 개요

**ITK (Insight Segmentation and Registration Toolkit)** 는 N차원 과학 영상 처리, 분할(Segmentation), 정합(Registration)을 위한 오픈소스 크로스플랫폼 툴킷입니다.

```
┌─────────────────────────────────────────────────────────────────┐
│                        ITK Architecture                         │
├─────────────────────────────────────────────────────────────────┤
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐        │
│  │  Reader  │→ │  Filter  │→ │  Filter  │→ │  Writer  │        │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘        │
│       ↑                                          ↓              │
│  ┌──────────┐                           ┌──────────────┐        │
│  │  DICOM   │                           │  Output      │        │
│  │  Files   │                           │  (NIfTI,etc) │        │
│  └──────────┘                           └──────────────────────┘│
│                                                                 │
│  Data Flow Architecture (Lazy Evaluation Pipeline)              │
└─────────────────────────────────────────────────────────────────┘
```

### 1.2 핵심 특징

| 특징 | 설명 |
|------|------|
| **N차원 지원** | 2D, 3D, 4D 이상의 다차원 영상 처리 |
| **데이터플로우 아키텍처** | 지연 평가(Lazy Evaluation) 기반 파이프라인 |
| **제네릭 프로그래밍** | C++ 템플릿 기반 타입 안전성 |
| **크로스플랫폼** | Windows, Linux, macOS 지원 |
| **다중 언어** | C++, Python (SimpleITK), Java, R 바인딩 |

### 1.3 주요 용도

- **CT/MRI 영상 분석**: 의료 영상 처리의 사실상 표준
- **영상 분할(Segmentation)**: 장기, 종양 등 관심 영역 추출
- **영상 정합(Registration)**: 다중 모달리티 영상 정렬
- **영상 필터링**: 노이즈 제거, 엣지 강화 등

---

## 2. 아키텍처

### 2.1 데이터플로우 파이프라인

ITK의 핵심은 **데이터플로우 아키텍처**입니다:

```cpp
// 파이프라인 예시: DICOM 읽기 → 가우시안 스무딩 → 저장
using ImageType = itk::Image<short, 3>;

// 1. Reader (Source)
auto reader = itk::ImageFileReader<ImageType>::New();
reader->SetFileName("input.dcm");

// 2. Filter (Process Object)
auto smoother = itk::DiscreteGaussianImageFilter<ImageType, ImageType>::New();
smoother->SetInput(reader->GetOutput());  // 파이프라인 연결
smoother->SetVariance(2.0);

// 3. Writer (Mapper)
auto writer = itk::ImageFileWriter<ImageType>::New();
writer->SetInput(smoother->GetOutput());
writer->SetFileName("output.nii");

// Update() 호출 시점에만 실제 연산 수행 (Lazy Evaluation)
writer->Update();
```

### 2.2 핵심 클래스 계층

```
itk::Object
├── itk::DataObject
│   ├── itk::Image<TPixel, VDimension>      // N차원 영상
│   ├── itk::Mesh<TPixel, VDimension>       // 메시 데이터
│   └── itk::PointSet<TPixel, VDimension>   // 포인트 집합
│
├── itk::ProcessObject
│   ├── itk::ImageSource (Sources)
│   │   ├── itk::ImageFileReader            // 파일 읽기
│   │   └── itk::ImageSeriesReader          // DICOM 시리즈
│   │
│   ├── itk::ImageToImageFilter (Filters)
│   │   ├── itk::DiscreteGaussianImageFilter
│   │   ├── itk::ThresholdImageFilter
│   │   └── itk::BinaryThresholdImageFilter
│   │
│   └── itk::ImageFileWriter (Mappers)      // 파일 쓰기
│
└── itk::Transform
    ├── itk::AffineTransform
    ├── itk::BSplineTransform
    └── itk::DisplacementFieldTransform
```

### 2.3 이미지 클래스 구조

```cpp
template <typename TPixel, unsigned int VImageDimension = 2>
class Image : public ImageBase<VImageDimension>
{
public:
    // 픽셀 타입 정의
    using PixelType = TPixel;
    using IndexType = itk::Index<VImageDimension>;
    using SizeType = itk::Size<VImageDimension>;
    using RegionType = itk::ImageRegion<VImageDimension>;
    using SpacingType = itk::Vector<double, VImageDimension>;
    using PointType = itk::Point<double, VImageDimension>;
    using DirectionType = itk::Matrix<double, VImageDimension, VImageDimension>;

    // 주요 메서드
    PixelType GetPixel(const IndexType& index) const;
    void SetPixel(const IndexType& index, const PixelType& value);

    // 공간 정보
    SpacingType GetSpacing() const;       // 복셀 간격 (mm)
    PointType GetOrigin() const;          // 원점 좌표
    DirectionType GetDirection() const;   // 방향 코사인
};
```

---

## 3. 영상 분할 (Segmentation) 알고리즘

### 3.1 분할 알고리즘 분류

```
┌────────────────────────────────────────────────────────────────┐
│                  ITK Segmentation Algorithms                   │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  ┌─────────────────┐  ┌─────────────────┐  ┌────────────────┐ │
│  │  Region-Based   │  │   Edge-Based    │  │   Deformable   │ │
│  │                 │  │                 │  │    Models      │ │
│  │  - Region       │  │  - Canny Edge   │  │                │ │
│  │    Growing      │  │  - Watershed    │  │  - Level Sets  │ │
│  │  - Connected    │  │  - Gradient     │  │  - Active      │ │
│  │    Component    │  │    Magnitude    │  │    Contours    │ │
│  │  - Confidence   │  │                 │  │  - Geodesic    │ │
│  │    Connected    │  │                 │  │    Active      │ │
│  └─────────────────┘  └─────────────────┘  └────────────────┘ │
│                                                                │
│  ┌─────────────────┐  ┌─────────────────┐                     │
│  │  Classification │  │   Morphology    │                     │
│  │                 │  │                 │                     │
│  │  - K-Means      │  │  - Erosion      │                     │
│  │  - MRF          │  │  - Dilation     │                     │
│  │  - Bayesian     │  │  - Opening      │                     │
│  │  - SVM          │  │  - Closing      │                     │
│  └─────────────────┘  └─────────────────┘                     │
└────────────────────────────────────────────────────────────────┘
```

### 3.2 주요 분할 필터

#### Region Growing

```cpp
// Connected Threshold: 시드 기반 영역 성장
auto regionGrowing = itk::ConnectedThresholdImageFilter<ImageType, ImageType>::New();
regionGrowing->SetInput(image);
regionGrowing->SetSeed(seedIndex);
regionGrowing->SetLower(lowerThreshold);
regionGrowing->SetUpper(upperThreshold);
regionGrowing->SetReplaceValue(255);
```

#### Level Set

```cpp
// Geodesic Active Contour Level Set
auto levelSet = itk::GeodesicActiveContourLevelSetImageFilter<ImageType, ImageType>::New();
levelSet->SetPropagationScaling(1.0);
levelSet->SetCurvatureScaling(1.0);
levelSet->SetAdvectionScaling(1.0);
levelSet->SetMaximumRMSError(0.02);
levelSet->SetNumberOfIterations(800);
```

#### Watershed

```cpp
// Watershed 분할
auto watershed = itk::WatershedImageFilter<ImageType>::New();
watershed->SetInput(gradientMagnitude->GetOutput());
watershed->SetThreshold(0.01);
watershed->SetLevel(0.2);
```

---

## 4. 영상 정합 (Registration) 프레임워크

### 4.1 정합 컴포넌트 구조

```
┌─────────────────────────────────────────────────────────────────┐
│                 ITK Registration Framework                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   Fixed Image ─────┐                                            │
│                    ├──→ ┌───────────┐                           │
│   Moving Image ────┤    │  Metric   │──→ Optimizer ──→ Transform│
│                    │    │ (Similar- │                           │
│   Transform ───────┤    │   ity)    │                           │
│                    │    └───────────┘                           │
│   Interpolator ────┘                                            │
│                                                                 │
│   Components:                                                   │
│   ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌───────────┐│
│   │ Transform   │ │Interpolator │ │   Metric    │ │ Optimizer ││
│   │             │ │             │ │             │ │           ││
│   │ - Rigid     │ │ - Linear    │ │ - MI        │ │ - GD      ││
│   │ - Affine    │ │ - BSpline   │ │ - NCC       │ │ - LBFGSB  ││
│   │ - BSpline   │ │ - Nearest   │ │ - MSE       │ │ - Powell  ││
│   │ - Deformable│ │ - Sinc      │ │ - Demons    │ │ - Amoeba  ││
│   └─────────────┘ └─────────────┘ └─────────────┘ └───────────┘│
└─────────────────────────────────────────────────────────────────┘
```

### 4.2 정합 예시 코드

```cpp
// 3D Affine Registration
using FixedImageType = itk::Image<float, 3>;
using MovingImageType = itk::Image<float, 3>;
using TransformType = itk::AffineTransform<double, 3>;
using OptimizerType = itk::RegularStepGradientDescentOptimizerv4<double>;
using MetricType = itk::MattesMutualInformationImageToImageMetricv4<
    FixedImageType, MovingImageType>;
using RegistrationType = itk::ImageRegistrationMethodv4<
    FixedImageType, MovingImageType, TransformType>;

auto registration = RegistrationType::New();
registration->SetFixedImage(fixedImage);
registration->SetMovingImage(movingImage);
registration->SetOptimizer(optimizer);
registration->SetMetric(metric);
registration->SetInitialTransform(initialTransform);
registration->Update();

// 결과 변환 획득
auto finalTransform = registration->GetModifiableTransform();
```

---

## 5. 영상 필터 (Filters)

### 5.1 필터 카테고리

| 카테고리 | 필터 예시 | 용도 |
|---------|----------|------|
| **Smoothing** | `DiscreteGaussian`, `Median`, `CurvatureAnisotropic` | 노이즈 제거 |
| **Edge Detection** | `CannyEdge`, `Sobel`, `LaplacianRecursiveGaussian` | 경계 검출 |
| **Morphology** | `BinaryErode`, `BinaryDilate`, `GrayscaleOpen` | 형태학적 연산 |
| **Thresholding** | `BinaryThreshold`, `OtsuThreshold`, `AdaptiveHistogram` | 이진화 |
| **Intensity** | `RescaleIntensity`, `HistogramMatching`, `N4BiasFieldCorrection` | 강도 보정 |
| **Geometric** | `Resample`, `Flip`, `Crop`, `Pad` | 기하학적 변환 |

### 5.2 주요 필터 사용 예시

```cpp
// Gaussian Smoothing
auto gaussian = itk::DiscreteGaussianImageFilter<ImageType, ImageType>::New();
gaussian->SetVariance(sigmaSquared);
gaussian->SetMaximumKernelWidth(32);

// Anisotropic Diffusion (엣지 보존 스무딩)
auto anisotropic = itk::CurvatureAnisotropicDiffusionImageFilter<
    ImageType, ImageType>::New();
anisotropic->SetTimeStep(0.0625);
anisotropic->SetNumberOfIterations(5);
anisotropic->SetConductanceParameter(3.0);

// Otsu Thresholding
auto otsu = itk::OtsuThresholdImageFilter<ImageType, ImageType>::New();
otsu->SetInsideValue(255);
otsu->SetOutsideValue(0);
```

---

## 6. DICOM 처리

### 6.1 DICOM 시리즈 읽기

```cpp
// DICOM 시리즈 읽기 (ITK 표준 방식)
using ReaderType = itk::ImageSeriesReader<ImageType>;
using NamesGeneratorType = itk::GDCMSeriesFileNames;

auto namesGenerator = NamesGeneratorType::New();
namesGenerator->SetDirectory(dicomDirectory);

// Series UID로 파일 목록 획득
auto seriesUID = namesGenerator->GetSeriesUIDs();
auto fileNames = namesGenerator->GetFileNames(seriesUID.front());

auto reader = ReaderType::New();
reader->SetFileNames(fileNames);

// GDCM ImageIO 사용
auto dicomIO = itk::GDCMImageIO::New();
reader->SetImageIO(dicomIO);
reader->Update();

// 메타데이터 접근
auto metaDict = dicomIO->GetMetaDataDictionary();
std::string patientName;
itk::ExposeMetaData<std::string>(metaDict, "0010|0010", patientName);
```

### 6.2 ITK의 LPS 좌표계

ITK는 **LPS (Left-Posterior-Superior)** 좌표계를 사용합니다:

```
       Superior (Z+)
            ↑
            │
            │
  Left ─────┼───── Right
  (X+)      │      (X-)
            │
            ↓
       Inferior (Z-)

       Posterior (Y+) ─── 환자 뒤쪽
       Anterior (Y-)  ─── 환자 앞쪽
```

| 축 | 방향 | DICOM 태그 참조 |
|----|------|-----------------|
| X | 환자 오른쪽 → 왼쪽 | Image Position (0020,0032) |
| Y | 환자 앞쪽 → 뒤쪽 | Image Orientation (0020,0037) |
| Z | 환자 발 → 머리 | Slice Location (0020,1041) |

---

## 7. 빌드 및 설치

### 7.1 CMake 설정

```cmake
cmake_minimum_required(VERSION 3.16)
project(DicomViewer)

# ITK 찾기
find_package(ITK REQUIRED COMPONENTS
    ITKCommon
    ITKIOImageBase
    ITKIOGDCM
    ITKImageFilterBase
    ITKImageFunction
    ITKSmoothing
    ITKThresholding
    ITKLevelSets
    ITKRegistrationCommon
    ITKRegistrationMethodsv4
    ITKMathematicalMorphology
    ITKVtkGlue  # VTK 연동 시
)
include(${ITK_USE_FILE})

add_executable(dicom_viewer main.cpp)
target_link_libraries(dicom_viewer ${ITK_LIBRARIES})
```

### 7.2 주요 ITK 모듈

| 모듈 | 설명 |
|------|------|
| `ITKCommon` | 기본 데이터 구조 |
| `ITKIOGDCM` | DICOM 입출력 (GDCM 기반) |
| `ITKSmoothing` | 스무딩 필터 |
| `ITKThresholding` | 임계값 처리 |
| `ITKLevelSets` | 레벨셋 분할 |
| `ITKRegistrationCommon` | 정합 기본 클래스 |
| `ITKMathematicalMorphology` | 수학적 형태학 |
| `ITKVtkGlue` | VTK 연동 어댑터 |

---

## 8. SimpleITK

### 8.1 개요

**SimpleITK**는 ITK의 간소화된 API로, 템플릿 없이 쉽게 사용할 수 있습니다:

```python
import SimpleITK as sitk

# DICOM 시리즈 읽기
reader = sitk.ImageSeriesReader()
dicom_names = reader.GetGDCMSeriesFileNames(dicom_dir)
reader.SetFileNames(dicom_names)
image = reader.Execute()

# Gaussian 스무딩
smoothed = sitk.DiscreteGaussian(image, variance=2.0)

# Otsu 임계값
binary = sitk.OtsuThreshold(smoothed, 0, 1)

# 저장
sitk.WriteImage(binary, "output.nii.gz")
```

### 8.2 C++ vs SimpleITK 비교

| 측면 | ITK C++ | SimpleITK |
|------|---------|-----------|
| **학습 곡선** | 가파름 (템플릿) | 완만함 |
| **유연성** | 매우 높음 | 제한적 |
| **성능** | 최적 | 약간의 오버헤드 |
| **파이프라인** | 명시적 | 암시적 |
| **언어** | C++ | Python, R, Java, C# 등 |

---

## 9. 참고 자료

### 공식 문서
- [ITK Software Guide](https://itk.org/ITKSoftwareGuide/html/)
- [ITK Doxygen API](https://docs.itk.org/projects/doxygen/en/v5.4.0/)
- [ITK Examples](https://examples.itk.org/)

### 학습 자료
- [ITK Courses](https://docs.itk.org/en/latest/learn/courses.html)
- [SimpleITK Notebooks](https://github.com/InsightSoftwareConsortium/SimpleITK-Notebooks)

### 관련 논문
- Ibanez, L., et al. "The ITK Software Guide." (Kitware, Inc.)
- Yoo, T.S., et al. "Engineering and Algorithm Design for an Image Processing API"

---

*다음 문서: [02-vtk-overview.md](02-vtk-overview.md) - VTK 개요 및 아키텍처*

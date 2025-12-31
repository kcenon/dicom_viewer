# DICOM 의료영상 처리 파이프라인

> **Last Updated**: 2025-12-31
> **Standards**: DICOM PS3.x (Part 3, 5, 6, 10, 18)

## 1. 개요

### 1.1 의료 영상 처리 파이프라인 구조

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Medical Image Processing Pipeline                     │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────┐   ┌─────────┐   ┌─────────┐   ┌─────────┐   ┌───────────┐│
│  │  Data   │ → │  Pre-   │ → │ Process │ → │  Post-  │ → │ Visuali-  ││
│  │ Loading │   │ process │   │  /Anal  │   │ process │   │  zation   ││
│  └─────────┘   └─────────┘   └─────────┘   └─────────┘   └───────────┘│
│       │             │             │             │               │       │
│       ↓             ↓             ↓             ↓               ↓       │
│  ┌─────────┐   ┌─────────┐   ┌─────────┐   ┌─────────┐   ┌───────────┐│
│  │ DICOM   │   │ Denoise │   │ Segment │   │ Surface │   │ Volume    ││
│  │ Parse   │   │ Resample│   │ Register│   │ Extract │   │ Render    ││
│  │ Metadata│   │ Normalize│   │ Measure │   │ Label   │   │ 2D/3D    ││
│  └─────────┘   └─────────┘   └─────────┘   └─────────┘   └───────────┘│
│                                                                         │
│  Technology:  pacs_system     ITK          ITK          ITK/VTK   VTK  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 1.2 모달리티별 특성

| 모달리티 | 픽셀 타입 | 값 범위 | 해상도 | 주요 용도 |
|----------|-----------|---------|--------|-----------|
| **CT** | Signed Short | -1024 ~ 3071 HU | 0.5-1.0 mm | 뼈, 폐, 혈관 |
| **MRI T1** | Unsigned Short | 신호 강도 | 0.5-1.5 mm | 연조직 구조 |
| **MRI T2** | Unsigned Short | 신호 강도 | 0.5-1.5 mm | 병변 검출 |
| **PET** | Float | SUV | 2-4 mm | 대사 활성 |
| **US** | Unsigned Char | 0-255 | 가변 | 실시간 영상 |
| **XR/DR** | Unsigned Short | 0-65535 | 0.1-0.2 mm | 2D 투영 |

---

## 2. 데이터 로딩 단계

### 2.1 DICOM 파일 구조

```
┌─────────────────────────────────────────────────────────────────┐
│                     DICOM File Structure                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   ┌─────────────────────────────────────────────────────────┐  │
│   │                 Preamble (128 bytes)                     │  │
│   │                  (Reserved, may be zero)                 │  │
│   └─────────────────────────────────────────────────────────┘  │
│   ┌─────────────────────────────────────────────────────────┐  │
│   │                 Prefix "DICM" (4 bytes)                  │  │
│   └─────────────────────────────────────────────────────────┘  │
│   ┌─────────────────────────────────────────────────────────┐  │
│   │           File Meta Information (Group 0002)             │  │
│   │                                                         │  │
│   │   (0002,0000) File Meta Information Length              │  │
│   │   (0002,0001) File Meta Information Version             │  │
│   │   (0002,0002) Media Storage SOP Class UID               │  │
│   │   (0002,0003) Media Storage SOP Instance UID            │  │
│   │   (0002,0010) Transfer Syntax UID                       │  │
│   │   (0002,0012) Implementation Class UID                  │  │
│   └─────────────────────────────────────────────────────────┘  │
│   ┌─────────────────────────────────────────────────────────┐  │
│   │                   Dataset (Main Data)                    │  │
│   │                                                         │  │
│   │   Patient Module (Group 0010)                           │  │
│   │   Study Module (Group 0020)                             │  │
│   │   Series Module (Group 0020)                            │  │
│   │   Image Module (Group 0028)                             │  │
│   │   Pixel Data (Group 7FE0)                               │  │
│   └─────────────────────────────────────────────────────────┘  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 핵심 DICOM 태그

#### 환자/연구 정보
| 태그 | 이름 | VR | 설명 |
|------|------|-----|------|
| (0010,0010) | Patient's Name | PN | 환자 이름 |
| (0010,0020) | Patient ID | LO | 환자 ID |
| (0010,0030) | Patient's Birth Date | DA | 생년월일 |
| (0010,0040) | Patient's Sex | CS | 성별 |
| (0020,000D) | Study Instance UID | UI | 연구 고유 ID |
| (0020,000E) | Series Instance UID | UI | 시리즈 고유 ID |
| (0008,0060) | Modality | CS | 장비 종류 |

#### 영상 기하학 정보
| 태그 | 이름 | VR | 설명 |
|------|------|-----|------|
| (0020,0032) | Image Position (Patient) | DS | 첫 픽셀의 물리적 좌표 (x,y,z) |
| (0020,0037) | Image Orientation (Patient) | DS | 행/열 방향 코사인 |
| (0028,0030) | Pixel Spacing | DS | 픽셀 간격 (row, col) |
| (0018,0050) | Slice Thickness | DS | 슬라이스 두께 |
| (0020,0013) | Instance Number | IS | 슬라이스 번호 |
| (0020,1041) | Slice Location | DS | 슬라이스 위치 |

#### 픽셀 데이터 정보
| 태그 | 이름 | VR | 설명 |
|------|------|-----|------|
| (0028,0010) | Rows | US | 행 수 (Height) |
| (0028,0011) | Columns | US | 열 수 (Width) |
| (0028,0100) | Bits Allocated | US | 할당 비트 수 (8, 16) |
| (0028,0101) | Bits Stored | US | 저장 비트 수 |
| (0028,0102) | High Bit | US | 최상위 비트 위치 |
| (0028,0103) | Pixel Representation | US | 0=unsigned, 1=signed |
| (0028,1050) | Window Center | DS | 윈도우 중심값 |
| (0028,1051) | Window Width | DS | 윈도우 폭 |
| (0028,1052) | Rescale Intercept | DS | 스케일 오프셋 |
| (0028,1053) | Rescale Slope | DS | 스케일 기울기 |
| (7FE0,0010) | Pixel Data | OW/OB | 실제 픽셀 데이터 |

### 2.3 Hounsfield Unit (HU) 변환

CT 영상의 경우 저장된 픽셀 값을 HU로 변환해야 합니다:

```
HU = Stored Value × Rescale Slope + Rescale Intercept
```

```cpp
// HU 변환 예시
double rescaleSlope = 1.0;      // (0028,1053)
double rescaleIntercept = -1024.0; // (0028,1052)

// 저장된 값 → HU
int16_t storedValue = 1024;
double hu = storedValue * rescaleSlope + rescaleIntercept;  // 0 HU (물)
```

**HU 기준값:**
| 조직 | HU 범위 |
|------|---------|
| 공기 | -1000 |
| 폐 | -500 ~ -900 |
| 지방 | -100 ~ -50 |
| 물 | 0 |
| 근육 | 10 ~ 40 |
| 혈액 | 30 ~ 45 |
| 간 | 40 ~ 60 |
| 뼈 (해면질) | 100 ~ 300 |
| 뼈 (치밀질) | 300 ~ 3000 |

---

## 3. 전처리 단계

### 3.1 리샘플링 (Resampling)

등방성(isotropic) 복셀로 변환:

```cpp
using ImageType = itk::Image<short, 3>;
using ResampleFilterType = itk::ResampleImageFilter<ImageType, ImageType>;
using TransformType = itk::IdentityTransform<double, 3>;
using InterpolatorType = itk::LinearInterpolateImageFunction<ImageType, double>;

auto resampleFilter = ResampleFilterType::New();
auto transform = TransformType::New();
auto interpolator = InterpolatorType::New();

// 목표 해상도 설정 (1mm 등방성)
ImageType::SpacingType outputSpacing;
outputSpacing.Fill(1.0);

// 새 크기 계산
ImageType::SizeType inputSize = inputImage->GetLargestPossibleRegion().GetSize();
ImageType::SpacingType inputSpacing = inputImage->GetSpacing();

ImageType::SizeType outputSize;
for (int i = 0; i < 3; ++i) {
    outputSize[i] = static_cast<unsigned int>(
        inputSize[i] * inputSpacing[i] / outputSpacing[i]
    );
}

resampleFilter->SetInput(inputImage);
resampleFilter->SetTransform(transform);
resampleFilter->SetInterpolator(interpolator);
resampleFilter->SetOutputSpacing(outputSpacing);
resampleFilter->SetOutputOrigin(inputImage->GetOrigin());
resampleFilter->SetOutputDirection(inputImage->GetDirection());
resampleFilter->SetSize(outputSize);
resampleFilter->Update();
```

### 3.2 노이즈 제거

```cpp
// Gaussian Smoothing (등방성 스무딩)
using GaussianFilterType = itk::DiscreteGaussianImageFilter<ImageType, ImageType>;
auto gaussianFilter = GaussianFilterType::New();
gaussianFilter->SetInput(image);
gaussianFilter->SetVariance(1.0);  // σ² = 1.0
gaussianFilter->Update();

// Anisotropic Diffusion (엣지 보존)
using AnisotropicFilterType = itk::CurvatureAnisotropicDiffusionImageFilter<
    ImageType, itk::Image<float, 3>>;
auto anisotropicFilter = AnisotropicFilterType::New();
anisotropicFilter->SetInput(image);
anisotropicFilter->SetTimeStep(0.0625);  // 3D 안정성 조건
anisotropicFilter->SetNumberOfIterations(5);
anisotropicFilter->SetConductanceParameter(3.0);
anisotropicFilter->Update();

// Median Filter (salt-and-pepper 노이즈)
using MedianFilterType = itk::MedianImageFilter<ImageType, ImageType>;
auto medianFilter = MedianFilterType::New();
ImageType::SizeType radius;
radius.Fill(1);  // 3x3x3 커널
medianFilter->SetRadius(radius);
medianFilter->SetInput(image);
medianFilter->Update();
```

### 3.3 강도 정규화

```cpp
// Intensity Windowing (CT용)
using IntensityWindowingFilterType = itk::IntensityWindowingImageFilter<
    ImageType, ImageType>;
auto windowingFilter = IntensityWindowingFilterType::New();
windowingFilter->SetInput(image);
windowingFilter->SetWindowMinimum(-1000);  // HU 최소
windowingFilter->SetWindowMaximum(3000);   // HU 최대
windowingFilter->SetOutputMinimum(0);
windowingFilter->SetOutputMaximum(255);
windowingFilter->Update();

// Histogram Equalization
using HistogramEqualizationType = itk::AdaptiveHistogramEqualizationImageFilter<ImageType>;
auto histogramFilter = HistogramEqualizationType::New();
histogramFilter->SetInput(image);
histogramFilter->SetAlpha(0.5);  // 대비 조절
histogramFilter->SetBeta(0.5);   // 평활화 정도
histogramFilter->Update();

// N4 Bias Field Correction (MRI용)
using N4FilterType = itk::N4BiasFieldCorrectionImageFilter<
    itk::Image<float, 3>, itk::Image<float, 3>>;
auto n4Filter = N4FilterType::New();
n4Filter->SetInput(mriImage);
n4Filter->SetMaskImage(brainMask);
n4Filter->SetNumberOfFittingLevels(4);
n4Filter->SetMaximumNumberOfIterations({50, 50, 50, 50});
n4Filter->Update();
```

---

## 4. 분할 단계 (Segmentation)

### 4.1 분할 알고리즘 선택 가이드

```
┌─────────────────────────────────────────────────────────────────┐
│              Segmentation Algorithm Selection Guide              │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   ┌─────────────────────────────────────────────────────────┐  │
│   │                    대상 특성 분석                        │  │
│   └─────────────────────────────────────────────────────────┘  │
│                            │                                    │
│              ┌─────────────┼─────────────┐                     │
│              ↓             ↓             ↓                      │
│   ┌──────────────┐ ┌──────────────┐ ┌──────────────┐           │
│   │ 경계 명확?   │ │ 강도 균일?   │ │ 복잡한 형태? │           │
│   └──────────────┘ └──────────────┘ └──────────────┘           │
│         │                  │                │                   │
│   ┌─────┴─────┐     ┌─────┴─────┐    ┌─────┴─────┐            │
│   ↓           ↓     ↓           ↓    ↓           ↓             │
│ ┌────┐    ┌────┐  ┌────┐    ┌────┐ ┌────┐    ┌────┐           │
│ │ Y  │    │ N  │  │ Y  │    │ N  │ │ Y  │    │ N  │           │
│ └────┘    └────┘  └────┘    └────┘ └────┘    └────┘           │
│   │         │       │         │      │         │               │
│   ↓         ↓       ↓         ↓      ↓         ↓               │
│ Threshold  Level  Region   Class-  Level   Simple             │
│  /Edge     Sets   Growing   ifier  Sets   Threshold           │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 4.2 임계값 분할 (Thresholding)

```cpp
// Otsu 자동 임계값
using OtsuFilterType = itk::OtsuThresholdImageFilter<ImageType, MaskImageType>;
auto otsuFilter = OtsuFilterType::New();
otsuFilter->SetInput(image);
otsuFilter->SetInsideValue(1);
otsuFilter->SetOutsideValue(0);
otsuFilter->Update();
auto threshold = otsuFilter->GetThreshold();

// Multi-Otsu (다중 클래스)
using MultiOtsuFilterType = itk::OtsuMultipleThresholdsImageFilter<
    ImageType, MaskImageType>;
auto multiOtsuFilter = MultiOtsuFilterType::New();
multiOtsuFilter->SetInput(image);
multiOtsuFilter->SetNumberOfThresholds(3);  // 4개 클래스로 분할
multiOtsuFilter->Update();
```

### 4.3 영역 성장 (Region Growing)

```cpp
// Connected Threshold
using ConnectedFilterType = itk::ConnectedThresholdImageFilter<
    ImageType, MaskImageType>;
auto connectedFilter = ConnectedFilterType::New();
connectedFilter->SetInput(image);
connectedFilter->SetLower(lowerThreshold);
connectedFilter->SetUpper(upperThreshold);
connectedFilter->SetReplaceValue(1);

// 시드 포인트 추가
ImageType::IndexType seed;
seed[0] = 100; seed[1] = 100; seed[2] = 50;
connectedFilter->SetSeed(seed);
connectedFilter->Update();

// Confidence Connected (자동 임계값)
using ConfidenceConnectedFilterType = itk::ConfidenceConnectedImageFilter<
    ImageType, MaskImageType>;
auto confidenceFilter = ConfidenceConnectedFilterType::New();
confidenceFilter->SetInput(image);
confidenceFilter->SetMultiplier(2.5);  // 표준편차 배수
confidenceFilter->SetNumberOfIterations(5);
confidenceFilter->SetInitialNeighborhoodRadius(2);
confidenceFilter->SetReplaceValue(1);
confidenceFilter->SetSeed(seed);
confidenceFilter->Update();
```

### 4.4 레벨셋 분할 (Level Sets)

```cpp
// Fast Marching + Geodesic Active Contour
using RealImageType = itk::Image<float, 3>;

// 1. 속도 이미지 생성 (Sigmoid)
using SigmoidFilterType = itk::SigmoidImageFilter<RealImageType, RealImageType>;
auto sigmoidFilter = SigmoidFilterType::New();
sigmoidFilter->SetInput(gradientMagnitude);
sigmoidFilter->SetAlpha(-alpha);
sigmoidFilter->SetBeta(beta);
sigmoidFilter->SetOutputMinimum(0.0);
sigmoidFilter->SetOutputMaximum(1.0);

// 2. Fast Marching으로 초기 레벨셋 생성
using FastMarchingFilterType = itk::FastMarchingImageFilter<
    RealImageType, RealImageType>;
auto fastMarching = FastMarchingFilterType::New();

using NodeType = FastMarchingFilterType::NodeType;
using NodeContainer = FastMarchingFilterType::NodeContainer;
auto seeds = NodeContainer::New();

NodeType node;
node.SetValue(-seedValue);
node.SetIndex(seedIndex);
seeds->Initialize();
seeds->InsertElement(0, node);

fastMarching->SetTrialPoints(seeds);
fastMarching->SetSpeedConstant(1.0);
fastMarching->SetOutputSize(image->GetLargestPossibleRegion().GetSize());

// 3. Geodesic Active Contour
using GeodesicActiveContourFilterType = itk::GeodesicActiveContourLevelSetImageFilter<
    RealImageType, RealImageType>;
auto geodesicActiveContour = GeodesicActiveContourFilterType::New();
geodesicActiveContour->SetInput(fastMarching->GetOutput());
geodesicActiveContour->SetFeatureImage(sigmoidFilter->GetOutput());
geodesicActiveContour->SetPropagationScaling(propagationScaling);
geodesicActiveContour->SetCurvatureScaling(curvatureScaling);
geodesicActiveContour->SetAdvectionScaling(advectionScaling);
geodesicActiveContour->SetMaximumRMSError(0.02);
geodesicActiveContour->SetNumberOfIterations(800);

// 4. 이진화
using ThresholdFilterType = itk::BinaryThresholdImageFilter<
    RealImageType, MaskImageType>;
auto thresholder = ThresholdFilterType::New();
thresholder->SetInput(geodesicActiveContour->GetOutput());
thresholder->SetLowerThreshold(-1000.0);
thresholder->SetUpperThreshold(0.0);
thresholder->SetOutsideValue(0);
thresholder->SetInsideValue(1);
```

### 4.5 형태학적 후처리

```cpp
using StructuringElementType = itk::BinaryBallStructuringElement<
    MaskPixelType, 3>;
StructuringElementType structuringElement;
structuringElement.SetRadius(2);
structuringElement.CreateStructuringElement();

// 열기 연산 (Opening): 작은 구멍 제거
using OpeningFilterType = itk::BinaryMorphologicalOpeningImageFilter<
    MaskImageType, MaskImageType, StructuringElementType>;
auto openingFilter = OpeningFilterType::New();
openingFilter->SetInput(binaryImage);
openingFilter->SetKernel(structuringElement);
openingFilter->SetForegroundValue(1);
openingFilter->Update();

// 닫기 연산 (Closing): 작은 물체 제거
using ClosingFilterType = itk::BinaryMorphologicalClosingImageFilter<
    MaskImageType, MaskImageType, StructuringElementType>;
auto closingFilter = ClosingFilterType::New();
closingFilter->SetInput(openingFilter->GetOutput());
closingFilter->SetKernel(structuringElement);
closingFilter->SetForegroundValue(1);
closingFilter->Update();

// Hole Filling
using FillHolesFilterType = itk::BinaryFillholeImageFilter<MaskImageType>;
auto fillHolesFilter = FillHolesFilterType::New();
fillHolesFilter->SetInput(closingFilter->GetOutput());
fillHolesFilter->SetForegroundValue(1);
fillHolesFilter->Update();
```

---

## 5. 정합 단계 (Registration)

### 5.1 정합 유형

```
┌─────────────────────────────────────────────────────────────────┐
│                    Registration Types                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   Rigid (6 DOF)           Affine (12 DOF)      Deformable       │
│   ┌───────────────┐       ┌───────────────┐   ┌───────────────┐│
│   │               │       │    Shear      │   │ ~~~~~~~~~~~~~ ││
│   │  Translation  │       │    Scale      │   │ ~ Free-form ~ ││
│   │  Rotation     │       │    +Rigid     │   │ ~ Deformation~││
│   │               │       │               │   │ ~~~~~~~~~~~~~ ││
│   └───────────────┘       └───────────────┘   └───────────────┘│
│                                                                 │
│   용도:                    용도:               용도:            │
│   - 다중 시점 정렬         - 스케일 차이 보정  - 해부학적 변형  │
│   - 다중 모달리티          - 촬영 조건 차이    - 아틀라스 정합  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2 다중 모달리티 정합 (CT-MRI)

```cpp
using FixedImageType = itk::Image<float, 3>;
using MovingImageType = itk::Image<float, 3>;

// Mutual Information Metric
using MetricType = itk::MattesMutualInformationImageToImageMetricv4<
    FixedImageType, MovingImageType>;
auto metric = MetricType::New();
metric->SetNumberOfHistogramBins(50);

// Rigid Transform
using TransformType = itk::VersorRigid3DTransform<double>;
auto initialTransform = TransformType::New();

// Center of geometry 초기화
using TransformInitializerType = itk::CenteredTransformInitializer<
    TransformType, FixedImageType, MovingImageType>;
auto initializer = TransformInitializerType::New();
initializer->SetTransform(initialTransform);
initializer->SetFixedImage(fixedImage);
initializer->SetMovingImage(movingImage);
initializer->GeometryOn();
initializer->InitializeTransform();

// Optimizer
using OptimizerType = itk::RegularStepGradientDescentOptimizerv4<double>;
auto optimizer = OptimizerType::New();
optimizer->SetLearningRate(1.0);
optimizer->SetMinimumStepLength(0.001);
optimizer->SetNumberOfIterations(200);
optimizer->SetRelaxationFactor(0.5);

// Registration
using RegistrationType = itk::ImageRegistrationMethodv4<
    FixedImageType, MovingImageType, TransformType>;
auto registration = RegistrationType::New();
registration->SetFixedImage(fixedImage);
registration->SetMovingImage(movingImage);
registration->SetOptimizer(optimizer);
registration->SetMetric(metric);
registration->SetInitialTransform(initialTransform);

// Multi-resolution
constexpr unsigned int numberOfLevels = 3;
using ShrinkFactorsType = RegistrationType::ShrinkFactorsPerDimensionContainerType;
ShrinkFactorsType shrinkFactors;
shrinkFactors[0] = 4; shrinkFactors[1] = 2; shrinkFactors[2] = 1;

RegistrationType::SmoothingSigmasArrayType smoothingSigmas;
smoothingSigmas.SetSize(3);
smoothingSigmas[0] = 2; smoothingSigmas[1] = 1; smoothingSigmas[2] = 0;

registration->SetNumberOfLevels(numberOfLevels);
registration->SetShrinkFactorsPerLevel(shrinkFactors);
registration->SetSmoothingSigmasPerLevel(smoothingSigmas);

registration->Update();
```

---

## 6. 측정 및 분석

### 6.1 볼륨 측정

```cpp
// Label Statistics
using LabelStatisticsFilterType = itk::LabelStatisticsImageFilter<
    ImageType, MaskImageType>;
auto labelStatistics = LabelStatisticsFilterType::New();
labelStatistics->SetInput(image);
labelStatistics->SetLabelInput(labelImage);
labelStatistics->Update();

// 볼륨 계산
double voxelVolume = spacing[0] * spacing[1] * spacing[2];  // mm³
unsigned long voxelCount = labelStatistics->GetCount(labelValue);
double volumeMm3 = voxelCount * voxelVolume;
double volumeCm3 = volumeMm3 / 1000.0;  // cc

// 통계값
double mean = labelStatistics->GetMean(labelValue);
double stdDev = labelStatistics->GetSigma(labelValue);
double min = labelStatistics->GetMinimum(labelValue);
double max = labelStatistics->GetMaximum(labelValue);
```

### 6.2 Shape Analysis

```cpp
using ShapeLabelMapType = itk::ShapeLabelMap<itk::ShapeLabelObject<
    MaskPixelType, 3>>;
using BinaryImageToShapeLabelMapFilterType =
    itk::BinaryImageToShapeLabelMapFilter<MaskImageType, ShapeLabelMapType>;

auto labelMapFilter = BinaryImageToShapeLabelMapFilterType::New();
labelMapFilter->SetInput(binaryMask);
labelMapFilter->Update();

auto labelMap = labelMapFilter->GetOutput();
auto labelObject = labelMap->GetLabelObject(1);

// Shape 특성
double volume = labelObject->GetPhysicalSize();  // mm³
double surfaceArea = labelObject->GetPerimeter();
double elongation = labelObject->GetElongation();
double roundness = labelObject->GetRoundness();
double sphericity = labelObject->GetFeretDiameter();

// Bounding Box
auto boundingBox = labelObject->GetBoundingBox();
```

---

## 7. 후처리 및 표면 추출

### 7.1 Marching Cubes (VTK)

```cpp
// ITK 마스크 → VTK PolyData
auto connector = ITKToVTKType::New();
connector->SetInput(binaryMask);
connector->Update();

// Marching Cubes
auto marchingCubes = vtkSmartPointer<vtkMarchingCubes>::New();
marchingCubes->SetInputData(connector->GetOutput());
marchingCubes->SetValue(0, 0.5);  // 이진 마스크의 등값면
marchingCubes->ComputeNormalsOn();
marchingCubes->Update();

// 스무딩
auto smoother = vtkSmartPointer<vtkWindowedSincPolyDataFilter>::New();
smoother->SetInputConnection(marchingCubes->GetOutputPort());
smoother->SetNumberOfIterations(15);
smoother->SetPassBand(0.1);
smoother->Update();

// Decimation
auto decimate = vtkSmartPointer<vtkDecimatePro>::New();
decimate->SetInputConnection(smoother->GetOutputPort());
decimate->SetTargetReduction(0.5);
decimate->PreserveTopologyOn();
decimate->Update();
```

### 7.2 STL 내보내기

```cpp
// STL Writer
auto stlWriter = vtkSmartPointer<vtkSTLWriter>::New();
stlWriter->SetInputConnection(decimate->GetOutputPort());
stlWriter->SetFileName("output.stl");
stlWriter->SetFileTypeToBinary();
stlWriter->Write();

// PLY Writer (색상 포함)
auto plyWriter = vtkSmartPointer<vtkPLYWriter>::New();
plyWriter->SetInputConnection(decimate->GetOutputPort());
plyWriter->SetFileName("output.ply");
plyWriter->SetColorModeToDefault();
plyWriter->Write();
```

---

## 8. 완전한 파이프라인 예제

```cpp
class MedicalImagePipeline
{
public:
    using PixelType = short;
    using ImageType = itk::Image<PixelType, 3>;
    using MaskType = itk::Image<unsigned char, 3>;
    using FloatImageType = itk::Image<float, 3>;

    struct PipelineResult {
        ImageType::Pointer preprocessedImage;
        MaskType::Pointer segmentationMask;
        vtkSmartPointer<vtkPolyData> surfaceMesh;
        double volumeCc;
    };

    PipelineResult Execute(const std::string& dicomDirectory)
    {
        PipelineResult result;

        // 1. DICOM 로딩
        auto image = LoadDICOM(dicomDirectory);

        // 2. 전처리
        auto smoothed = ApplySmoothing(image, 1.0);
        auto resampled = ResampleIsotropic(smoothed, 1.0);
        result.preprocessedImage = resampled;

        // 3. 분할 (예: CT 뼈)
        auto boneMask = ThresholdSegment(resampled, 200, 3000);
        auto cleanedMask = MorphologicalCleanup(boneMask);
        result.segmentationMask = cleanedMask;

        // 4. 측정
        result.volumeCc = CalculateVolume(cleanedMask);

        // 5. 표면 추출
        result.surfaceMesh = ExtractSurface(cleanedMask);

        return result;
    }

private:
    // 각 단계별 구현...
};
```

---

## 9. 참고 자료

### DICOM 표준
- [DICOM Standard Browser](https://dicom.innolitics.com/)
- [DICOM PS3.3 - Information Object Definitions](https://dicom.nema.org/medical/dicom/current/output/html/part03.html)

### 의료 영상 처리
- [ITK Software Guide - Book 2](https://itk.org/ITKSoftwareGuide/html/Book2/ITKSoftwareGuide-Book2ch4.html)
- [3D Slicer Tutorials](https://www.slicer.org/wiki/Documentation/Nightly/Training)

---

*이전 문서: [03-itk-vtk-integration.md](03-itk-vtk-integration.md) - ITK-VTK 통합*
*다음 문서: [05-pacs-integration.md](05-pacs-integration.md) - pacs_system 연동 가이드*

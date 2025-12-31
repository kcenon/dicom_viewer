# DICOM Medical Image Processing Pipeline

> **Last Updated**: 2025-12-31
> **Standards**: DICOM PS3.x (Part 3, 5, 6, 10, 18)

## 1. Overview

### 1.1 Medical Image Processing Pipeline Structure

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

### 1.2 Modality Characteristics

| Modality | Pixel Type | Value Range | Resolution | Primary Use |
|----------|-----------|---------|--------|-----------|
| **CT** | Signed Short | -1024 ~ 3071 HU | 0.5-1.0 mm | Bone, Lung, Vessels |
| **MRI T1** | Unsigned Short | Signal Intensity | 0.5-1.5 mm | Soft Tissue Structure |
| **MRI T2** | Unsigned Short | Signal Intensity | 0.5-1.5 mm | Lesion Detection |
| **PET** | Float | SUV | 2-4 mm | Metabolic Activity |
| **US** | Unsigned Char | 0-255 | Variable | Real-time Imaging |
| **XR/DR** | Unsigned Short | 0-65535 | 0.1-0.2 mm | 2D Projection |

---

## 2. Data Loading Stage

### 2.1 DICOM File Structure

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

### 2.2 Core DICOM Tags

#### Patient/Study Information
| Tag | Name | VR | Description |
|------|------|-----|------|
| (0010,0010) | Patient's Name | PN | Patient name |
| (0010,0020) | Patient ID | LO | Patient identifier |
| (0010,0030) | Patient's Birth Date | DA | Birth date |
| (0010,0040) | Patient's Sex | CS | Gender |
| (0020,000D) | Study Instance UID | UI | Unique study identifier |
| (0020,000E) | Series Instance UID | UI | Unique series identifier |
| (0008,0060) | Modality | CS | Equipment type |

#### Image Geometry Information
| Tag | Name | VR | Description |
|------|------|-----|------|
| (0020,0032) | Image Position (Patient) | DS | Physical coordinates of first pixel (x,y,z) |
| (0020,0037) | Image Orientation (Patient) | DS | Row/column direction cosines |
| (0028,0030) | Pixel Spacing | DS | Pixel spacing (row, col) |
| (0018,0050) | Slice Thickness | DS | Slice thickness |
| (0020,0013) | Instance Number | IS | Slice number |
| (0020,1041) | Slice Location | DS | Slice location |

#### Pixel Data Information
| Tag | Name | VR | Description |
|------|------|-----|------|
| (0028,0010) | Rows | US | Number of rows (Height) |
| (0028,0011) | Columns | US | Number of columns (Width) |
| (0028,0100) | Bits Allocated | US | Allocated bits (8, 16) |
| (0028,0101) | Bits Stored | US | Stored bits |
| (0028,0102) | High Bit | US | Position of high bit |
| (0028,0103) | Pixel Representation | US | 0=unsigned, 1=signed |
| (0028,1050) | Window Center | DS | Window center value |
| (0028,1051) | Window Width | DS | Window width |
| (0028,1052) | Rescale Intercept | DS | Scale offset |
| (0028,1053) | Rescale Slope | DS | Scale slope |
| (7FE0,0010) | Pixel Data | OW/OB | Actual pixel data |

### 2.3 Hounsfield Unit (HU) Conversion

For CT images, stored pixel values must be converted to HU:

```
HU = Stored Value × Rescale Slope + Rescale Intercept
```

```cpp
// HU conversion example
double rescaleSlope = 1.0;      // (0028,1053)
double rescaleIntercept = -1024.0; // (0028,1052)

// Stored value → HU
int16_t storedValue = 1024;
double hu = storedValue * rescaleSlope + rescaleIntercept;  // 0 HU (water)
```

**HU Reference Values:**
| Tissue | HU Range |
|------|---------|
| Air | -1000 |
| Lung | -500 ~ -900 |
| Fat | -100 ~ -50 |
| Water | 0 |
| Muscle | 10 ~ 40 |
| Blood | 30 ~ 45 |
| Liver | 40 ~ 60 |
| Bone (cancellous) | 100 ~ 300 |
| Bone (cortical) | 300 ~ 3000 |

---

## 3. Preprocessing Stage

### 3.1 Resampling

Convert to isotropic voxels:

```cpp
using ImageType = itk::Image<short, 3>;
using ResampleFilterType = itk::ResampleImageFilter<ImageType, ImageType>;
using TransformType = itk::IdentityTransform<double, 3>;
using InterpolatorType = itk::LinearInterpolateImageFunction<ImageType, double>;

auto resampleFilter = ResampleFilterType::New();
auto transform = TransformType::New();
auto interpolator = InterpolatorType::New();

// Set target resolution (1mm isotropic)
ImageType::SpacingType outputSpacing;
outputSpacing.Fill(1.0);

// Calculate new size
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

### 3.2 Noise Removal

```cpp
// Gaussian Smoothing (isotropic smoothing)
using GaussianFilterType = itk::DiscreteGaussianImageFilter<ImageType, ImageType>;
auto gaussianFilter = GaussianFilterType::New();
gaussianFilter->SetInput(image);
gaussianFilter->SetVariance(1.0);  // σ² = 1.0
gaussianFilter->Update();

// Anisotropic Diffusion (edge preserving)
using AnisotropicFilterType = itk::CurvatureAnisotropicDiffusionImageFilter<
    ImageType, itk::Image<float, 3>>;
auto anisotropicFilter = AnisotropicFilterType::New();
anisotropicFilter->SetInput(image);
anisotropicFilter->SetTimeStep(0.0625);  // 3D stability condition
anisotropicFilter->SetNumberOfIterations(5);
anisotropicFilter->SetConductanceParameter(3.0);
anisotropicFilter->Update();

// Median Filter (salt-and-pepper noise)
using MedianFilterType = itk::MedianImageFilter<ImageType, ImageType>;
auto medianFilter = MedianFilterType::New();
ImageType::SizeType radius;
radius.Fill(1);  // 3x3x3 kernel
medianFilter->SetRadius(radius);
medianFilter->SetInput(image);
medianFilter->Update();
```

### 3.3 Intensity Normalization

```cpp
// Intensity Windowing (for CT)
using IntensityWindowingFilterType = itk::IntensityWindowingImageFilter<
    ImageType, ImageType>;
auto windowingFilter = IntensityWindowingFilterType::New();
windowingFilter->SetInput(image);
windowingFilter->SetWindowMinimum(-1000);  // HU minimum
windowingFilter->SetWindowMaximum(3000);   // HU maximum
windowingFilter->SetOutputMinimum(0);
windowingFilter->SetOutputMaximum(255);
windowingFilter->Update();

// Histogram Equalization
using HistogramEqualizationType = itk::AdaptiveHistogramEqualizationImageFilter<ImageType>;
auto histogramFilter = HistogramEqualizationType::New();
histogramFilter->SetInput(image);
histogramFilter->SetAlpha(0.5);  // Contrast adjustment
histogramFilter->SetBeta(0.5);   // Equalization degree
histogramFilter->Update();

// N4 Bias Field Correction (for MRI)
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

## 4. Segmentation Stage

### 4.1 Segmentation Algorithm Selection Guide

```
┌─────────────────────────────────────────────────────────────────┐
│              Segmentation Algorithm Selection Guide              │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   ┌─────────────────────────────────────────────────────────┐  │
│   │                    Target Analysis                       │  │
│   └─────────────────────────────────────────────────────────┘  │
│                            │                                    │
│              ┌─────────────┼─────────────┐                     │
│              ↓             ↓             ↓                      │
│   ┌──────────────┐ ┌──────────────┐ ┌──────────────┐           │
│   │ Clear        │ │ Uniform      │ │ Complex      │           │
│   │ Boundaries?  │ │ Intensity?   │ │ Shape?       │           │
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

### 4.2 Thresholding

```cpp
// Otsu automatic threshold
using OtsuFilterType = itk::OtsuThresholdImageFilter<ImageType, MaskImageType>;
auto otsuFilter = OtsuFilterType::New();
otsuFilter->SetInput(image);
otsuFilter->SetInsideValue(1);
otsuFilter->SetOutsideValue(0);
otsuFilter->Update();
auto threshold = otsuFilter->GetThreshold();

// Multi-Otsu (multiple classes)
using MultiOtsuFilterType = itk::OtsuMultipleThresholdsImageFilter<
    ImageType, MaskImageType>;
auto multiOtsuFilter = MultiOtsuFilterType::New();
multiOtsuFilter->SetInput(image);
multiOtsuFilter->SetNumberOfThresholds(3);  // Segment into 4 classes
multiOtsuFilter->Update();
```

### 4.3 Region Growing

```cpp
// Connected Threshold
using ConnectedFilterType = itk::ConnectedThresholdImageFilter<
    ImageType, MaskImageType>;
auto connectedFilter = ConnectedFilterType::New();
connectedFilter->SetInput(image);
connectedFilter->SetLower(lowerThreshold);
connectedFilter->SetUpper(upperThreshold);
connectedFilter->SetReplaceValue(1);

// Add seed point
ImageType::IndexType seed;
seed[0] = 100; seed[1] = 100; seed[2] = 50;
connectedFilter->SetSeed(seed);
connectedFilter->Update();

// Confidence Connected (automatic threshold)
using ConfidenceConnectedFilterType = itk::ConfidenceConnectedImageFilter<
    ImageType, MaskImageType>;
auto confidenceFilter = ConfidenceConnectedFilterType::New();
confidenceFilter->SetInput(image);
confidenceFilter->SetMultiplier(2.5);  // Standard deviation multiplier
confidenceFilter->SetNumberOfIterations(5);
confidenceFilter->SetInitialNeighborhoodRadius(2);
confidenceFilter->SetReplaceValue(1);
confidenceFilter->SetSeed(seed);
confidenceFilter->Update();
```

### 4.4 Level Set Segmentation

```cpp
// Fast Marching + Geodesic Active Contour
using RealImageType = itk::Image<float, 3>;

// 1. Generate speed image (Sigmoid)
using SigmoidFilterType = itk::SigmoidImageFilter<RealImageType, RealImageType>;
auto sigmoidFilter = SigmoidFilterType::New();
sigmoidFilter->SetInput(gradientMagnitude);
sigmoidFilter->SetAlpha(-alpha);
sigmoidFilter->SetBeta(beta);
sigmoidFilter->SetOutputMinimum(0.0);
sigmoidFilter->SetOutputMaximum(1.0);

// 2. Generate initial level set with Fast Marching
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

// 4. Binarization
using ThresholdFilterType = itk::BinaryThresholdImageFilter<
    RealImageType, MaskImageType>;
auto thresholder = ThresholdFilterType::New();
thresholder->SetInput(geodesicActiveContour->GetOutput());
thresholder->SetLowerThreshold(-1000.0);
thresholder->SetUpperThreshold(0.0);
thresholder->SetOutsideValue(0);
thresholder->SetInsideValue(1);
```

### 4.5 Morphological Post-processing

```cpp
using StructuringElementType = itk::BinaryBallStructuringElement<
    MaskPixelType, 3>;
StructuringElementType structuringElement;
structuringElement.SetRadius(2);
structuringElement.CreateStructuringElement();

// Opening: Remove small holes
using OpeningFilterType = itk::BinaryMorphologicalOpeningImageFilter<
    MaskImageType, MaskImageType, StructuringElementType>;
auto openingFilter = OpeningFilterType::New();
openingFilter->SetInput(binaryImage);
openingFilter->SetKernel(structuringElement);
openingFilter->SetForegroundValue(1);
openingFilter->Update();

// Closing: Remove small objects
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

## 5. Registration Stage

### 5.1 Registration Types

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
│   Use cases:              Use cases:          Use cases:        │
│   - Multi-temporal        - Scale difference  - Anatomical      │
│     alignment               correction          deformation     │
│   - Multi-modality        - Acquisition       - Atlas           │
│                             condition diff.     registration    │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2 Multi-modality Registration (CT-MRI)

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

// Center of geometry initialization
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

## 6. Measurement and Analysis

### 6.1 Volume Measurement

```cpp
// Label Statistics
using LabelStatisticsFilterType = itk::LabelStatisticsImageFilter<
    ImageType, MaskImageType>;
auto labelStatistics = LabelStatisticsFilterType::New();
labelStatistics->SetInput(image);
labelStatistics->SetLabelInput(labelImage);
labelStatistics->Update();

// Volume calculation
double voxelVolume = spacing[0] * spacing[1] * spacing[2];  // mm³
unsigned long voxelCount = labelStatistics->GetCount(labelValue);
double volumeMm3 = voxelCount * voxelVolume;
double volumeCm3 = volumeMm3 / 1000.0;  // cc

// Statistics
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

// Shape characteristics
double volume = labelObject->GetPhysicalSize();  // mm³
double surfaceArea = labelObject->GetPerimeter();
double elongation = labelObject->GetElongation();
double roundness = labelObject->GetRoundness();
double sphericity = labelObject->GetFeretDiameter();

// Bounding Box
auto boundingBox = labelObject->GetBoundingBox();
```

---

## 7. Post-processing and Surface Extraction

### 7.1 Marching Cubes (VTK)

```cpp
// ITK mask → VTK PolyData
auto connector = ITKToVTKType::New();
connector->SetInput(binaryMask);
connector->Update();

// Marching Cubes
auto marchingCubes = vtkSmartPointer<vtkMarchingCubes>::New();
marchingCubes->SetInputData(connector->GetOutput());
marchingCubes->SetValue(0, 0.5);  // Isosurface of binary mask
marchingCubes->ComputeNormalsOn();
marchingCubes->Update();

// Smoothing
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

### 7.2 STL Export

```cpp
// STL Writer
auto stlWriter = vtkSmartPointer<vtkSTLWriter>::New();
stlWriter->SetInputConnection(decimate->GetOutputPort());
stlWriter->SetFileName("output.stl");
stlWriter->SetFileTypeToBinary();
stlWriter->Write();

// PLY Writer (with color)
auto plyWriter = vtkSmartPointer<vtkPLYWriter>::New();
plyWriter->SetInputConnection(decimate->GetOutputPort());
plyWriter->SetFileName("output.ply");
plyWriter->SetColorModeToDefault();
plyWriter->Write();
```

---

## 8. Complete Pipeline Example

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

        // 1. DICOM Loading
        auto image = LoadDICOM(dicomDirectory);

        // 2. Preprocessing
        auto smoothed = ApplySmoothing(image, 1.0);
        auto resampled = ResampleIsotropic(smoothed, 1.0);
        result.preprocessedImage = resampled;

        // 3. Segmentation (e.g., CT bone)
        auto boneMask = ThresholdSegment(resampled, 200, 3000);
        auto cleanedMask = MorphologicalCleanup(boneMask);
        result.segmentationMask = cleanedMask;

        // 4. Measurement
        result.volumeCc = CalculateVolume(cleanedMask);

        // 5. Surface Extraction
        result.surfaceMesh = ExtractSurface(cleanedMask);

        return result;
    }

private:
    // Implementation for each stage...
};
```

---

## 9. References

### DICOM Standard
- [DICOM Standard Browser](https://dicom.innolitics.com/)
- [DICOM PS3.3 - Information Object Definitions](https://dicom.nema.org/medical/dicom/current/output/html/part03.html)

### Medical Image Processing
- [ITK Software Guide - Book 2](https://itk.org/ITKSoftwareGuide/html/Book2/ITKSoftwareGuide-Book2ch4.html)
- [3D Slicer Tutorials](https://www.slicer.org/wiki/Documentation/Nightly/Training)

---

*Previous Document: [03-itk-vtk-integration.md](03-itk-vtk-integration.md) - ITK-VTK Integration*
*Next Document: [05-pacs-integration.md](05-pacs-integration.md) - pacs_system Integration Guide*

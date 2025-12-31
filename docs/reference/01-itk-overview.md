# ITK (Insight Toolkit) Overview and Architecture

> **Version**: ITK 5.x
> **Last Updated**: 2025-12-31
> **Reference**: [ITK Official Site](https://itk.org/), [ITK GitHub](https://github.com/InsightSoftwareConsortium/ITK)

## 1. Introduction to ITK

### 1.1 Overview

**ITK (Insight Segmentation and Registration Toolkit)** is an open-source cross-platform toolkit for N-dimensional scientific image processing, segmentation, and registration.

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

### 1.2 Key Features

| Feature | Description |
|---------|-------------|
| **N-Dimensional Support** | Processing of 2D, 3D, 4D and higher dimensional images |
| **Dataflow Architecture** | Pipeline based on lazy evaluation |
| **Generic Programming** | Type safety through C++ templates |
| **Cross-Platform** | Support for Windows, Linux, macOS |
| **Multi-Language** | C++, Python (SimpleITK), Java, R bindings |

### 1.3 Primary Use Cases

- **CT/MRI Image Analysis**: De facto standard for medical image processing
- **Image Segmentation**: Extraction of regions of interest such as organs and tumors
- **Image Registration**: Alignment of multi-modality images
- **Image Filtering**: Noise removal, edge enhancement, etc.

---

## 2. Architecture

### 2.1 Dataflow Pipeline

The core of ITK is its **dataflow architecture**:

```cpp
// Pipeline example: DICOM reading → Gaussian smoothing → Save
using ImageType = itk::Image<short, 3>;

// 1. Reader (Source)
auto reader = itk::ImageFileReader<ImageType>::New();
reader->SetFileName("input.dcm");

// 2. Filter (Process Object)
auto smoother = itk::DiscreteGaussianImageFilter<ImageType, ImageType>::New();
smoother->SetInput(reader->GetOutput());  // Pipeline connection
smoother->SetVariance(2.0);

// 3. Writer (Mapper)
auto writer = itk::ImageFileWriter<ImageType>::New();
writer->SetInput(smoother->GetOutput());
writer->SetFileName("output.nii");

// Actual computation only occurs when Update() is called (Lazy Evaluation)
writer->Update();
```

### 2.2 Core Class Hierarchy

```
itk::Object
├── itk::DataObject
│   ├── itk::Image<TPixel, VDimension>      // N-dimensional image
│   ├── itk::Mesh<TPixel, VDimension>       // Mesh data
│   └── itk::PointSet<TPixel, VDimension>   // Point set
│
├── itk::ProcessObject
│   ├── itk::ImageSource (Sources)
│   │   ├── itk::ImageFileReader            // File reading
│   │   └── itk::ImageSeriesReader          // DICOM series
│   │
│   ├── itk::ImageToImageFilter (Filters)
│   │   ├── itk::DiscreteGaussianImageFilter
│   │   ├── itk::ThresholdImageFilter
│   │   └── itk::BinaryThresholdImageFilter
│   │
│   └── itk::ImageFileWriter (Mappers)      // File writing
│
└── itk::Transform
    ├── itk::AffineTransform
    ├── itk::BSplineTransform
    └── itk::DisplacementFieldTransform
```

### 2.3 Image Class Structure

```cpp
template <typename TPixel, unsigned int VImageDimension = 2>
class Image : public ImageBase<VImageDimension>
{
public:
    // Pixel type definitions
    using PixelType = TPixel;
    using IndexType = itk::Index<VImageDimension>;
    using SizeType = itk::Size<VImageDimension>;
    using RegionType = itk::ImageRegion<VImageDimension>;
    using SpacingType = itk::Vector<double, VImageDimension>;
    using PointType = itk::Point<double, VImageDimension>;
    using DirectionType = itk::Matrix<double, VImageDimension, VImageDimension>;

    // Main methods
    PixelType GetPixel(const IndexType& index) const;
    void SetPixel(const IndexType& index, const PixelType& value);

    // Spatial information
    SpacingType GetSpacing() const;       // Voxel spacing (mm)
    PointType GetOrigin() const;          // Origin coordinates
    DirectionType GetDirection() const;   // Direction cosines
};
```

---

## 3. Image Segmentation Algorithms

### 3.1 Segmentation Algorithm Classification

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

### 3.2 Key Segmentation Filters

#### Region Growing

```cpp
// Connected Threshold: Seed-based region growing
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
// Watershed Segmentation
auto watershed = itk::WatershedImageFilter<ImageType>::New();
watershed->SetInput(gradientMagnitude->GetOutput());
watershed->SetThreshold(0.01);
watershed->SetLevel(0.2);
```

---

## 4. Image Registration Framework

### 4.1 Registration Component Structure

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

### 4.2 Registration Example Code

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

// Obtain final transform
auto finalTransform = registration->GetModifiableTransform();
```

---

## 5. Image Filters

### 5.1 Filter Categories

| Category | Filter Examples | Purpose |
|----------|----------------|---------|
| **Smoothing** | `DiscreteGaussian`, `Median`, `CurvatureAnisotropic` | Noise removal |
| **Edge Detection** | `CannyEdge`, `Sobel`, `LaplacianRecursiveGaussian` | Boundary detection |
| **Morphology** | `BinaryErode`, `BinaryDilate`, `GrayscaleOpen` | Morphological operations |
| **Thresholding** | `BinaryThreshold`, `OtsuThreshold`, `AdaptiveHistogram` | Binarization |
| **Intensity** | `RescaleIntensity`, `HistogramMatching`, `N4BiasFieldCorrection` | Intensity correction |
| **Geometric** | `Resample`, `Flip`, `Crop`, `Pad` | Geometric transformations |

### 5.2 Key Filter Usage Examples

```cpp
// Gaussian Smoothing
auto gaussian = itk::DiscreteGaussianImageFilter<ImageType, ImageType>::New();
gaussian->SetVariance(sigmaSquared);
gaussian->SetMaximumKernelWidth(32);

// Anisotropic Diffusion (Edge-preserving smoothing)
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

## 6. DICOM Processing

### 6.1 Reading DICOM Series

```cpp
// Reading DICOM series (standard ITK approach)
using ReaderType = itk::ImageSeriesReader<ImageType>;
using NamesGeneratorType = itk::GDCMSeriesFileNames;

auto namesGenerator = NamesGeneratorType::New();
namesGenerator->SetDirectory(dicomDirectory);

// Get file list by Series UID
auto seriesUID = namesGenerator->GetSeriesUIDs();
auto fileNames = namesGenerator->GetFileNames(seriesUID.front());

auto reader = ReaderType::New();
reader->SetFileNames(fileNames);

// Use GDCM ImageIO
auto dicomIO = itk::GDCMImageIO::New();
reader->SetImageIO(dicomIO);
reader->Update();

// Access metadata
auto metaDict = dicomIO->GetMetaDataDictionary();
std::string patientName;
itk::ExposeMetaData<std::string>(metaDict, "0010|0010", patientName);
```

### 6.2 ITK's LPS Coordinate System

ITK uses the **LPS (Left-Posterior-Superior)** coordinate system:

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

       Posterior (Y+) ─── Patient's back
       Anterior (Y-)  ─── Patient's front
```

| Axis | Direction | DICOM Tag Reference |
|------|-----------|---------------------|
| X | Patient's right → left | Image Position (0020,0032) |
| Y | Patient's front → back | Image Orientation (0020,0037) |
| Z | Patient's feet → head | Slice Location (0020,1041) |

---

## 7. Build and Installation

### 7.1 CMake Configuration

```cmake
cmake_minimum_required(VERSION 3.16)
project(DicomViewer)

# Find ITK
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
    ITKVtkGlue  # For VTK integration
)
include(${ITK_USE_FILE})

add_executable(dicom_viewer main.cpp)
target_link_libraries(dicom_viewer ${ITK_LIBRARIES})
```

### 7.2 Key ITK Modules

| Module | Description |
|--------|-------------|
| `ITKCommon` | Basic data structures |
| `ITKIOGDCM` | DICOM I/O (GDCM-based) |
| `ITKSmoothing` | Smoothing filters |
| `ITKThresholding` | Threshold processing |
| `ITKLevelSets` | Level set segmentation |
| `ITKRegistrationCommon` | Registration base classes |
| `ITKMathematicalMorphology` | Mathematical morphology |
| `ITKVtkGlue` | VTK integration adapter |

---

## 8. SimpleITK

### 8.1 Overview

**SimpleITK** is a simplified API for ITK that can be used easily without templates:

```python
import SimpleITK as sitk

# Read DICOM series
reader = sitk.ImageSeriesReader()
dicom_names = reader.GetGDCMSeriesFileNames(dicom_dir)
reader.SetFileNames(dicom_names)
image = reader.Execute()

# Gaussian smoothing
smoothed = sitk.DiscreteGaussian(image, variance=2.0)

# Otsu thresholding
binary = sitk.OtsuThreshold(smoothed, 0, 1)

# Save
sitk.WriteImage(binary, "output.nii.gz")
```

### 8.2 C++ vs SimpleITK Comparison

| Aspect | ITK C++ | SimpleITK |
|--------|---------|-----------|
| **Learning Curve** | Steep (templates) | Gentle |
| **Flexibility** | Very high | Limited |
| **Performance** | Optimal | Slight overhead |
| **Pipeline** | Explicit | Implicit |
| **Languages** | C++ | Python, R, Java, C#, etc. |

---

## 9. References

### Official Documentation
- [ITK Software Guide](https://itk.org/ITKSoftwareGuide/html/)
- [ITK Doxygen API](https://docs.itk.org/projects/doxygen/en/v5.4.0/)
- [ITK Examples](https://examples.itk.org/)

### Learning Resources
- [ITK Courses](https://docs.itk.org/en/latest/learn/courses.html)
- [SimpleITK Notebooks](https://github.com/InsightSoftwareConsortium/SimpleITK-Notebooks)

### Related Papers
- Ibanez, L., et al. "The ITK Software Guide." (Kitware, Inc.)
- Yoo, T.S., et al. "Engineering and Algorithm Design for an Image Processing API"

---

*Next document: [02-vtk-overview.md](02-vtk-overview.md) - VTK Overview and Architecture*

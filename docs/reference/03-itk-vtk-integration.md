# ITK-VTK Integration Guide

> **Last Updated**: 2025-12-31
> **Reference**: [ITK VtkGlue Module](https://github.com/InsightSoftwareConsortium/ITK/tree/master/Modules/Bridge/VtkGlue)

## 1. Overview

### 1.1 Role Division Between ITK and VTK

ITK and VTK each have unique strengths and are used complementarily in medical image processing:

```
┌─────────────────────────────────────────────────────────────────┐
│              ITK + VTK Integration Architecture                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   ┌─────────────── ITK ───────────────┐                        │
│   │                                   │                        │
│   │  • DICOM read/write               │                        │
│   │  • Image filtering (Smoothing,    │                        │
│   │    Edge detection)                │                        │
│   │  • Image segmentation             │                        │
│   │  • Image registration             │                        │
│   │  • Spatial coordinate transform   │                        │
│   │                                   │                        │
│   │         itk::Image<T, D>          │                        │
│   └───────────────┬───────────────────┘                        │
│                   │                                             │
│                   ↓ ITKVtkGlue                                  │
│                   │                                             │
│   ┌───────────────┴───────────────────┐                        │
│   │                                   │                        │
│   │  • 2D/3D visualization            │                        │
│   │  • Volume rendering               │                        │
│   │  • Surface rendering              │                        │
│   │  • Interactive widgets            │                        │
│   │  • GUI integration (Qt)           │                        │
│   │                                   │                        │
│   │         vtkImageData              │                        │
│   └─────────────── VTK ───────────────┘                        │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 1.2 Why Integration is Needed

| Feature | ITK | VTK |
|---------|-----|-----|
| **DICOM Reading** | ✅ Full support (GDCM) | ⚠️ Limited |
| **Image Segmentation** | ✅ Various algorithms | ❌ Basic only |
| **Image Registration** | ✅ Complete framework | ❌ None |
| **Filtering** | ✅ Medical-specific | ✅ General purpose |
| **3D Rendering** | ❌ None | ✅ Full support |
| **Volume Rendering** | ❌ None | ✅ GPU accelerated |
| **Interaction** | ❌ None | ✅ Widget library |
| **GUI Integration** | ❌ None | ✅ Qt/Tk |

---

## 2. Data Conversion

### 2.1 ITKVtkGlue Module

ITK's `ITKVtkGlue` module handles data conversion between ITK and VTK:

```cpp
// Required headers
#include <itkImageToVTKImageFilter.h>
#include <itkVTKImageToImageFilter.h>
```

### 2.2 ITK → VTK Conversion

```cpp
#include <itkImage.h>
#include <itkImageToVTKImageFilter.h>
#include <vtkImageData.h>

using ImageType = itk::Image<short, 3>;
using ConnectorType = itk::ImageToVTKImageFilter<ImageType>;

// Read ITK image
auto reader = itk::ImageFileReader<ImageType>::New();
reader->SetFileName("ct_image.dcm");
reader->Update();

// ITK → VTK conversion
auto connector = ConnectorType::New();
connector->SetInput(reader->GetOutput());
connector->Update();

// Get VTK image data
vtkImageData* vtkImage = connector->GetOutput();

// Note: If connector goes out of scope, vtkImage becomes invalid
// Use DeepCopy if needed:
auto safeImage = vtkSmartPointer<vtkImageData>::New();
safeImage->DeepCopy(connector->GetOutput());
```

### 2.3 VTK → ITK Conversion

```cpp
#include <itkVTKImageToImageFilter.h>

using ImageType = itk::Image<short, 3>;
using ConnectorType = itk::VTKImageToImageFilter<ImageType>;

// Convert VTK image to ITK image
auto connector = ConnectorType::New();
connector->SetInput(vtkImageData);
connector->Update();

// Get ITK image
ImageType::Pointer itkImage = connector->GetOutput();

// Disconnect from pipeline (optional)
itkImage->DisconnectPipeline();
```

### 2.4 Data Type Mapping

| ITK Type | VTK Type | Use Case |
|----------|----------|----------|
| `itk::Image<short, 3>` | `VTK_SHORT` | CT images (HU) |
| `itk::Image<unsigned short, 3>` | `VTK_UNSIGNED_SHORT` | General medical images |
| `itk::Image<float, 3>` | `VTK_FLOAT` | Precision computation, MRI |
| `itk::Image<unsigned char, 3>` | `VTK_UNSIGNED_CHAR` | Masks, labels |
| `itk::Image<itk::RGBPixel<unsigned char>, 3>` | `VTK_UNSIGNED_CHAR` (3 comp) | Color images |

---

## 3. Coordinate System Integration

### 3.1 Coordinate System Differences

Both ITK and VTK use the **LPS (Left-Posterior-Superior)** coordinate system, but there are differences in internal representation:

```
┌─────────────────────────────────────────────────────────────────┐
│                    Coordinate System Comparison                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│    ITK Image                         VTK ImageData              │
│    ─────────                         ──────────────             │
│                                                                 │
│    • Origin: First pixel center      • Origin: First pixel      │
│    • Direction Matrix: 3x3             center                   │
│    • Spacing: mm units               • Direction: Not supported*│
│                                      • Spacing: mm units        │
│                                                                 │
│    Physical Point = Origin           Physical Point = Origin    │
│                    + Direction        + Index * Spacing         │
│                    * Index                                      │
│                    * Spacing                                    │
│                                                                 │
│    * Direction support added in VTK 9.0+                        │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 Direction Matrix Handling

```cpp
// Get ITK image Direction Matrix
auto direction = itkImage->GetDirection();

// Apply Direction in VTK (VTK 9.0+)
#if VTK_MAJOR_VERSION >= 9
    double directionMatrix[9];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            directionMatrix[i * 3 + j] = direction[i][j];
        }
    }
    vtkImage->SetDirectionMatrix(directionMatrix);
#else
    // VTK 8.x and below: Use vtkTransform or vtkMatrix4x4
    auto transform = vtkSmartPointer<vtkTransform>::New();
    // ... Direction application logic
#endif
```

### 3.3 Coordinate Transformation Utilities

```cpp
// Physical Point → Index conversion
template<typename ImageType>
typename ImageType::IndexType
PhysicalToIndex(typename ImageType::Pointer image,
                typename ImageType::PointType point)
{
    typename ImageType::IndexType index;
    image->TransformPhysicalPointToIndex(point, index);
    return index;
}

// Index → Physical Point conversion
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

## 4. Integrated Pipeline Implementation

### 4.1 Basic Integration Pipeline

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

    // Read DICOM series (ITK)
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

    // ITK Filtering
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

    // ITK Segmentation
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

    // ITK → VTK conversion and rendering
    void RenderVolume(ImageType::Pointer itkImage)
    {
        // ITK → VTK conversion
        using ConnectorType = itk::ImageToVTKImageFilter<ImageType>;
        auto connector = ConnectorType::New();
        connector->SetInput(itkImage);
        connector->Update();

        // VTK volume rendering setup
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

        // Rendering
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

### 4.2 Usage Example

```cpp
int main(int argc, char* argv[])
{
    IntegratedPipeline pipeline;

    // 1. Read DICOM (ITK)
    auto image = pipeline.ReadDICOMSeries("/path/to/dicom");

    // 2. Preprocessing (ITK)
    auto smoothed = pipeline.ApplyGaussianSmoothing(image, 1.0);

    // 3. Segmentation (ITK)
    auto boneMask = pipeline.ThresholdSegmentation(smoothed, 200, 3000);

    // 4. Visualization (VTK)
    pipeline.RenderVolume(smoothed);

    return 0;
}
```

---

## 5. Mesh Conversion

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

    // Copy points
    for (auto it = mesh->GetPoints()->Begin();
         it != mesh->GetPoints()->End(); ++it)
    {
        auto point = it->Value();
        points->InsertNextPoint(point[0], point[1], point[2]);
    }

    // Copy cells (triangles)
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

    // Copy points
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

    // Copy cells
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

## 6. Advanced Integration Patterns

### 6.1 Bidirectional Update Pattern

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

### 6.2 Pipeline Caching

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

## 7. CMake Build Configuration

### 7.1 Integrated Build Configuration

```cmake
cmake_minimum_required(VERSION 3.16)
project(ITKVTKIntegration)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find ITK
find_package(ITK REQUIRED COMPONENTS
    ITKCommon
    ITKIOImageBase
    ITKIOGDCM
    ITKImageFilterBase
    ITKSmoothing
    ITKThresholding
    ITKVtkGlue          # ITK-VTK bridge module
)
include(${ITK_USE_FILE})

# Find VTK
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

# Executable
add_executable(itk_vtk_viewer
    main.cpp
    IntegratedPipeline.cpp
)

target_link_libraries(itk_vtk_viewer PRIVATE
    ${ITK_LIBRARIES}
    ${VTK_LIBRARIES}
)

# VTK auto-initialization
vtk_module_autoinit(
    TARGETS itk_vtk_viewer
    MODULES ${VTK_LIBRARIES}
)
```

### 7.2 vcpkg Configuration (Optional)

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

## 8. Performance Considerations

### 8.1 Memory Management

```cpp
// Recommended: Avoid unnecessary copies
auto connector = ITKToVTKType::New();
connector->SetInput(itkImage);
// Use GetOutput() directly - no copy
volumeMapper->SetInputData(connector->GetOutput());

// Note: connector lifetime management required
// If connector is destroyed, GetOutput() becomes invalid

// Use DeepCopy if independent copy is needed
auto safeCopy = vtkSmartPointer<vtkImageData>::New();
safeCopy->DeepCopy(connector->GetOutput());
```

### 8.2 Pipeline Optimization

```cpp
// Streaming processing (large data)
auto reader = itk::ImageFileReader<ImageType>::New();
reader->SetFileName("large_image.nrrd");
reader->UseStreamingOn();

// Enable multithreading
itk::MultiThreaderBase::SetGlobalDefaultNumberOfThreads(
    std::thread::hardware_concurrency()
);

// Check VTK GPU rendering support
auto volumeMapper = vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
if (!volumeMapper->IsRenderSupported(renderWindow, volumeProperty)) {
    // CPU fallback
    auto cpuMapper = vtkSmartPointer<vtkFixedPointVolumeRayCastMapper>::New();
    // ...
}
```

---

## 9. References

### Official Documentation
- [ITK VtkGlue Documentation](https://itk.org/ITKSoftwareGuide/html/Book1/ITKSoftwareGuide-Book1ch3.html)
- [VTK Examples with ITK](https://examples.vtk.org/)

### Related Projects
- [MITK](https://www.mitk.org/) - ITK+VTK integration framework
- [3D Slicer](https://www.slicer.org/) - Medical imaging platform
- [ITK-SNAP](http://www.itksnap.org/) - Segmentation tool

---

*Previous document: [02-vtk-overview.md](02-vtk-overview.md) - VTK Overview*
*Next document: [04-dicom-pipeline.md](04-dicom-pipeline.md) - DICOM Medical Image Processing Pipeline*

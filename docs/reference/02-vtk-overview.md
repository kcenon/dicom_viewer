# VTK (Visualization Toolkit) Overview and Architecture

> **Version**: VTK 9.x
> **Last Updated**: 2025-12-31
> **Reference**: [VTK Official Site](https://vtk.org/), [VTK GitHub](https://github.com/Kitware/VTK)

## 1. Introduction to VTK

### 1.1 Overview

**VTK (Visualization Toolkit)** is an open-source software system for 3D computer graphics, image processing, and scientific visualization. It is distributed under the BSD 3-Clause license.

```
┌─────────────────────────────────────────────────────────────────┐
│                      VTK Visualization Pipeline                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   ┌────────┐    ┌────────┐    ┌────────┐    ┌────────────────┐ │
│   │ Source │ →  │ Filter │ →  │ Mapper │ →  │     Actor      │ │
│   │        │    │        │    │        │    │                │ │
│   │ Reader │    │ Process│    │ Geometry│    │ Props in Scene │ │
│   │ Generate│    │ Data   │    │ to GPU  │    │                │ │
│   └────────┘    └────────┘    └────────┘    └────────────────┘ │
│                                                      │          │
│                                                      ↓          │
│                                              ┌────────────────┐ │
│                                              │   Renderer     │ │
│                                              │                │ │
│                                              │ Camera, Lights │ │
│                                              │ RenderWindow   │ │
│                                              └────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

### 1.2 Key Features

| Feature | Description |
|---------|-------------|
| **3D Rendering** | High-quality rendering based on OpenGL |
| **Volume Rendering** | Ray Casting with GPU acceleration |
| **Surface Rendering** | Marching Cubes, Isosurface extraction |
| **Interactive Widgets** | Widget library for 3D manipulation |
| **Multi-Platform** | Windows, Linux, macOS, Web (WebAssembly) |

### 1.3 Medical Imaging Applications

VTK is at the core of the following medical imaging applications:

- **3D Slicer**: Medical image analysis platform
- **ParaView**: Large-scale data visualization
- **MITK**: Medical Imaging Interaction Toolkit
- **ITK-SNAP**: Image segmentation tool

---

## 2. Architecture

### 2.1 Rendering Pipeline

VTK's visualization pipeline consists of two main parts: the **Data Pipeline** and the **Rendering Pipeline**:

```
┌─────────────────────────────────────────────────────────────────┐
│                    VTK Pipeline Architecture                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────── Data Pipeline ───────────┐   ┌── Render Pipeline │
│  │                                     │   │                   │
│  │  vtkDataSource                      │   │  vtkMapper        │
│  │       │                             │   │       │           │
│  │       ↓                             │   │       ↓           │
│  │  vtkAlgorithm (Filter)              │   │  vtkActor         │
│  │       │                             │   │       │           │
│  │       ↓                             │   │       ↓           │
│  │  vtkDataObject                      │→→→│  vtkRenderer      │
│  │  (vtkImageData, vtkPolyData, etc.)  │   │       │           │
│  │                                     │   │       ↓           │
│  └─────────────────────────────────────┘   │  vtkRenderWindow  │
│                                            │       │           │
│                                            │       ↓           │
│                                            │  vtkInteractor    │
│                                            └───────────────────┘
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 Core Class Hierarchy

```
vtkObjectBase
├── vtkObject
│   ├── vtkDataObject
│   │   ├── vtkDataSet
│   │   │   ├── vtkImageData          // 3D regular grid (CT/MRI)
│   │   │   ├── vtkRectilinearGrid    // Irregular spacing grid
│   │   │   ├── vtkStructuredGrid     // Structured grid
│   │   │   ├── vtkUnstructuredGrid   // Unstructured grid
│   │   │   └── vtkPolyData           // Polygon mesh
│   │   └── vtkTable, vtkTree, ...
│   │
│   ├── vtkAlgorithm
│   │   ├── vtkImageAlgorithm         // Image processing
│   │   │   ├── vtkImageReader2       // Image reading
│   │   │   ├── vtkDICOMImageReader   // DICOM reading
│   │   │   └── vtkImageGaussianSmooth
│   │   ├── vtkPolyDataAlgorithm      // Polygon processing
│   │   │   ├── vtkMarchingCubes      // Isosurface extraction
│   │   │   └── vtkContourFilter
│   │   └── vtkVolumeMapper           // Volume rendering
│   │
│   ├── vtkMapper
│   │   ├── vtkPolyDataMapper         // Polygon mapping
│   │   ├── vtkDataSetMapper          // Dataset mapping
│   │   └── vtkVolumeMapper           // Volume mapping
│   │       ├── vtkGPUVolumeRayCastMapper
│   │       └── vtkSmartVolumeMapper
│   │
│   ├── vtkProp
│   │   ├── vtkActor                  // Surface rendering
│   │   ├── vtkVolume                 // Volume rendering
│   │   └── vtkActor2D                // 2D elements
│   │
│   ├── vtkRenderer                   // Scene renderer
│   ├── vtkRenderWindow               // Render window
│   └── vtkRenderWindowInteractor     // Interaction handling
│
└── vtkSmartPointer<T>                // Smart pointer
```

### 2.3 vtkImageData Structure

The most commonly used data type in medical imaging:

```cpp
// vtkImageData: 3D regular grid data
class vtkImageData : public vtkDataSet
{
public:
    // Dimension information
    int* GetDimensions();              // [width, height, depth]
    void GetSpacing(double spacing[3]); // Voxel spacing (mm)
    void GetOrigin(double origin[3]);   // Origin coordinates

    // Pixel access
    void* GetScalarPointer(int x, int y, int z);
    double GetScalarComponentAsDouble(int x, int y, int z, int component);

    // Scalar information
    int GetScalarType();               // VTK_SHORT, VTK_FLOAT, etc.
    int GetNumberOfScalarComponents(); // Number of channels
};
```

---

## 3. Volume Rendering

### 3.1 Volume Rendering Pipeline

```
┌─────────────────────────────────────────────────────────────────┐
│                   Volume Rendering Pipeline                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   vtkImageData                                                  │
│        │                                                        │
│        ↓                                                        │
│   ┌─────────────────────────────────────────┐                   │
│   │         vtkVolumeMapper                 │                   │
│   │                                         │                   │
│   │  ┌─────────────────────────────────┐   │                   │
│   │  │ vtkGPUVolumeRayCastMapper       │   │  ← GPU accelerated│
│   │  │ vtkFixedPointVolumeRayCastMapper│   │  ← CPU            │
│   │  │ vtkSmartVolumeMapper            │   │  ← Auto selection │
│   │  └─────────────────────────────────┘   │                   │
│   └─────────────────────────────────────────┘                   │
│        │                                                        │
│        ↓                                                        │
│   ┌─────────────────────────────────────────┐                   │
│   │         vtkVolumeProperty               │                   │
│   │                                         │                   │
│   │  - Color Transfer Function              │                   │
│   │  - Opacity Transfer Function            │                   │
│   │  - Gradient Opacity                     │                   │
│   │  - Shading (Ambient, Diffuse, Specular) │                   │
│   └─────────────────────────────────────────┘                   │
│        │                                                        │
│        ↓                                                        │
│   vtkVolume → vtkRenderer → vtkRenderWindow                     │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 Ray Casting Volume Rendering

```cpp
#include <vtkSmartPointer.h>
#include <vtkDICOMImageReader.h>
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkVolumeProperty.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>
#include <vtkVolume.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>

// Read DICOM
auto reader = vtkSmartPointer<vtkDICOMImageReader>::New();
reader->SetDirectoryName(dicomDirectory);
reader->Update();

// GPU Ray Casting Mapper
auto volumeMapper = vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
volumeMapper->SetInputConnection(reader->GetOutputPort());
volumeMapper->SetBlendModeToComposite();

// Color Transfer Function (CT example)
auto colorFunc = vtkSmartPointer<vtkColorTransferFunction>::New();
colorFunc->AddRGBPoint(-1000, 0.0, 0.0, 0.0);  // Air: black
colorFunc->AddRGBPoint(-600, 0.62, 0.36, 0.18); // Fat: brown
colorFunc->AddRGBPoint(40, 0.88, 0.60, 0.29);   // Skin: flesh
colorFunc->AddRGBPoint(400, 1.0, 1.0, 0.9);     // Bone: white

// Opacity Transfer Function
auto opacityFunc = vtkSmartPointer<vtkPiecewiseFunction>::New();
opacityFunc->AddPoint(-1000, 0.0);
opacityFunc->AddPoint(-600, 0.0);
opacityFunc->AddPoint(-400, 0.15);
opacityFunc->AddPoint(400, 0.85);
opacityFunc->AddPoint(2000, 1.0);

// Volume Property
auto volumeProperty = vtkSmartPointer<vtkVolumeProperty>::New();
volumeProperty->SetColor(colorFunc);
volumeProperty->SetScalarOpacity(opacityFunc);
volumeProperty->SetInterpolationTypeToLinear();
volumeProperty->ShadeOn();
volumeProperty->SetAmbient(0.4);
volumeProperty->SetDiffuse(0.6);
volumeProperty->SetSpecular(0.2);

// Volume Actor
auto volume = vtkSmartPointer<vtkVolume>::New();
volume->SetMapper(volumeMapper);
volume->SetProperty(volumeProperty);

// Rendering
auto renderer = vtkSmartPointer<vtkRenderer>::New();
renderer->AddVolume(volume);
renderer->SetBackground(0.1, 0.1, 0.1);

auto renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
renderWindow->AddRenderer(renderer);
renderWindow->SetSize(800, 600);

auto interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
interactor->SetRenderWindow(renderWindow);
interactor->Start();
```

### 3.3 Transfer Function Presets

| Preset | Purpose | HU Range |
|--------|---------|----------|
| **CT Bone** | Bone visualization | 200 ~ 3000 HU |
| **CT Soft Tissue** | Soft tissue | -100 ~ 200 HU |
| **CT Lung** | Lung | -1000 ~ -400 HU |
| **CT Angio** | Vascular angiography | 100 ~ 500 HU |
| **MRI Default** | General MRI | Signal intensity based |

---

## 4. Surface Rendering

### 4.1 Marching Cubes Algorithm

The most widely used algorithm for isosurface extraction:

```cpp
#include <vtkMarchingCubes.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>

// Extract isosurface with Marching Cubes
auto marchingCubes = vtkSmartPointer<vtkMarchingCubes>::New();
marchingCubes->SetInputConnection(reader->GetOutputPort());
marchingCubes->SetValue(0, 400);  // Bone threshold in CT (HU)
marchingCubes->ComputeNormalsOn();
marchingCubes->ComputeGradientsOff();

// Smoothing (optional)
auto smoother = vtkSmartPointer<vtkSmoothPolyDataFilter>::New();
smoother->SetInputConnection(marchingCubes->GetOutputPort());
smoother->SetNumberOfIterations(50);
smoother->SetRelaxationFactor(0.1);

// Polygon reduction (optional)
auto decimate = vtkSmartPointer<vtkDecimatePro>::New();
decimate->SetInputConnection(smoother->GetOutputPort());
decimate->SetTargetReduction(0.5);  // 50% reduction
decimate->PreserveTopologyOn();

// Mapper and Actor
auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
mapper->SetInputConnection(decimate->GetOutputPort());
mapper->ScalarVisibilityOff();

auto actor = vtkSmartPointer<vtkActor>::New();
actor->SetMapper(mapper);
actor->GetProperty()->SetColor(0.9, 0.9, 0.8);  // Bone color
actor->GetProperty()->SetOpacity(1.0);
```

### 4.2 Multiple Isosurface Extraction

```cpp
// Extract multiple organs simultaneously
auto contourFilter = vtkSmartPointer<vtkContourFilter>::New();
contourFilter->SetInputConnection(reader->GetOutputPort());
contourFilter->SetValue(0, -500);  // Lung
contourFilter->SetValue(1, 100);   // Soft tissue
contourFilter->SetValue(2, 400);   // Bone
contourFilter->GenerateValues(3, -500, 400);  // Or specify range
```

---

## 5. 2D Slice Viewer

### 5.1 MPR (Multi-Planar Reconstruction)

```
┌─────────────────────────────────────────────────────────────────┐
│                      MPR Views                                  │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│    ┌────────────┐   ┌────────────┐   ┌────────────┐            │
│    │   Axial    │   │  Coronal   │   │  Sagittal  │            │
│    │   (XY)     │   │   (XZ)     │   │    (YZ)    │            │
│    │            │   │            │   │            │            │
│    │  ───────   │   │    ─┼─    │   │    │──     │            │
│    │  Top-Down  │   │  Front    │   │   Side     │            │
│    └────────────┘   └────────────┘   └────────────┘            │
│                                                                 │
│    Use vtkImageReslice to extract slices from arbitrary planes  │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2 Using vtkImageViewer2

```cpp
#include <vtkImageViewer2.h>
#include <vtkInteractorStyleImage.h>

// Basic 2D image viewer
auto viewer = vtkSmartPointer<vtkImageViewer2>::New();
viewer->SetInputConnection(reader->GetOutputPort());
viewer->SetSliceOrientationToXY();  // Axial
viewer->SetSlice(viewer->GetSliceMax() / 2);  // Middle slice

// Window/Level settings
viewer->SetColorWindow(400);  // CT window width
viewer->SetColorLevel(40);    // CT window level

// Connect render window
auto renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
viewer->SetRenderWindow(renderWindow);

auto interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
viewer->SetupInteractor(interactor);
viewer->Render();
interactor->Start();
```

### 5.3 Implementing MPR with vtkImageReslice

```cpp
#include <vtkImageReslice.h>
#include <vtkMatrix4x4.h>

// Reslice to arbitrary plane
auto reslice = vtkSmartPointer<vtkImageReslice>::New();
reslice->SetInputConnection(reader->GetOutputPort());
reslice->SetOutputDimensionality(2);
reslice->SetInterpolationModeToLinear();

// Axial (default)
auto axialMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
axialMatrix->DeepCopy(new double[16]{
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, slicePosition,
    0, 0, 0, 1
});
reslice->SetResliceAxes(axialMatrix);

// Coronal
auto coronalMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
coronalMatrix->DeepCopy(new double[16]{
    1, 0, 0, 0,
    0, 0, 1, 0,
    0, -1, 0, slicePosition,
    0, 0, 0, 1
});

// Sagittal
auto sagittalMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
sagittalMatrix->DeepCopy(new double[16]{
    0, 0, 1, 0,
    1, 0, 0, 0,
    0, -1, 0, slicePosition,
    0, 0, 0, 1
});
```

---

## 6. Interactive Widgets

### 6.1 Key Widget Classes

| Widget | Purpose |
|--------|---------|
| `vtkSliderWidget` | Slider (slice selection) |
| `vtkPlaneWidget` | Plane manipulation (MPR) |
| `vtkBoxWidget2` | Volume clipping |
| `vtkLineWidget2` | Distance measurement |
| `vtkAngleWidget` | Angle measurement |
| `vtkImagePlaneWidget` | Image plane |
| `vtkContourWidget` | Contour drawing |
| `vtkSeedWidget` | Seed points |

### 6.2 Widget Usage Example

```cpp
#include <vtkPlaneWidget.h>
#include <vtkImagePlaneWidget.h>

// Image plane widget (MPR interaction)
auto planeWidget = vtkSmartPointer<vtkImagePlaneWidget>::New();
planeWidget->SetInteractor(interactor);
planeWidget->SetInputConnection(reader->GetOutputPort());
planeWidget->SetPlaneOrientationToXAxes();
planeWidget->SetSliceIndex(100);
planeWidget->DisplayTextOn();
planeWidget->On();

// Slider widget
auto sliderRep = vtkSmartPointer<vtkSliderRepresentation2D>::New();
sliderRep->SetMinimumValue(0);
sliderRep->SetMaximumValue(reader->GetOutput()->GetDimensions()[2] - 1);
sliderRep->SetValue(50);
sliderRep->SetTitleText("Slice");
sliderRep->GetPoint1Coordinate()->SetCoordinateSystemToNormalizedDisplay();
sliderRep->GetPoint1Coordinate()->SetValue(0.1, 0.1);
sliderRep->GetPoint2Coordinate()->SetCoordinateSystemToNormalizedDisplay();
sliderRep->GetPoint2Coordinate()->SetValue(0.9, 0.1);

auto sliderWidget = vtkSmartPointer<vtkSliderWidget>::New();
sliderWidget->SetInteractor(interactor);
sliderWidget->SetRepresentation(sliderRep);
sliderWidget->EnabledOn();
```

---

## 7. DICOM Processing

### 7.1 vtkDICOMImageReader

VTK's basic DICOM reader (limited):

```cpp
auto reader = vtkSmartPointer<vtkDICOMImageReader>::New();
reader->SetDirectoryName(dicomDir);
reader->Update();

// Metadata access
std::string patientName = reader->GetPatientName();
float* spacing = reader->GetPixelSpacing();
float* origin = reader->GetImagePositionPatient();
int* dimensions = reader->GetOutput()->GetDimensions();
```

### 7.2 vtk-dicom Library (Recommended)

For more complete DICOM support, use [vtk-dicom](https://github.com/dgobbi/vtk-dicom):

```cpp
#include <vtkDICOMReader.h>
#include <vtkDICOMSorter.h>
#include <vtkDICOMMetaData.h>

// Sort DICOM files
auto sorter = vtkSmartPointer<vtkDICOMSorter>::New();
sorter->SetInputFileNames(fileNames);
sorter->Update();

// Read sorted series
auto reader = vtkSmartPointer<vtkDICOMReader>::New();
reader->SetFileNames(sorter->GetOutputFileNames());
reader->Update();

// Metadata
vtkDICOMMetaData* meta = reader->GetMetaData();
std::string patientID = meta->Get(DC::PatientID).AsString();
std::string studyDate = meta->Get(DC::StudyDate).AsString();
```

---

## 8. Qt Integration

### 8.1 QVTKOpenGLNativeWidget

```cpp
#include <QVTKOpenGLNativeWidget.h>
#include <vtkGenericOpenGLRenderWindow.h>

class DicomViewer : public QMainWindow
{
    Q_OBJECT
public:
    DicomViewer(QWidget* parent = nullptr)
    {
        // Qt widget setup
        m_vtkWidget = new QVTKOpenGLNativeWidget(this);
        setCentralWidget(m_vtkWidget);

        // VTK render window
        auto renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
        m_vtkWidget->setRenderWindow(renderWindow);

        // Add renderer
        m_renderer = vtkSmartPointer<vtkRenderer>::New();
        renderWindow->AddRenderer(m_renderer);

        // Setup interactor
        auto interactor = m_vtkWidget->interactor();
        auto style = vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
        interactor->SetInteractorStyle(style);
    }

private:
    QVTKOpenGLNativeWidget* m_vtkWidget;
    vtkSmartPointer<vtkRenderer> m_renderer;
};
```

### 8.2 CMake Configuration

```cmake
find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets)
find_package(VTK REQUIRED COMPONENTS
    CommonCore
    CommonDataModel
    IOImage
    IOXML
    InteractionStyle
    RenderingCore
    RenderingOpenGL2
    RenderingVolumeOpenGL2
    GUISupportQt
)

target_link_libraries(dicom_viewer PRIVATE
    Qt6::Widgets
    Qt6::OpenGLWidgets
    ${VTK_LIBRARIES}
)

vtk_module_autoinit(
    TARGETS dicom_viewer
    MODULES ${VTK_LIBRARIES}
)
```

---

## 9. Performance Optimization

### 9.1 GPU Acceleration

| Feature | Class | Notes |
|---------|-------|-------|
| **Volume Rendering** | `vtkGPUVolumeRayCastMapper` | CUDA/OpenGL |
| **Smart Mapper** | `vtkSmartVolumeMapper` | Auto GPU/CPU selection |
| **OpenGL Polygon** | `vtkOpenGLPolyDataMapper` | Default GPU acceleration |

### 9.2 Large Data Processing

```cpp
// Streaming processing
auto reader = vtkSmartPointer<vtkDICOMImageReader>::New();
reader->FileLowerLeftOn();
reader->SetDataExtent(0, 511, 0, 511, 0, 99);  // Partial loading
reader->Update();

// LOD (Level of Detail)
auto lodMapper = vtkSmartPointer<vtkLODProp3D>::New();
lodMapper->AddLOD(highResMapper, 0.0);
lodMapper->AddLOD(lowResMapper, 0.5);
```

---

## 10. Build Configuration

### 10.1 CMake Configuration

```cmake
cmake_minimum_required(VERSION 3.16)
project(DicomViewer)

find_package(VTK REQUIRED COMPONENTS
    # Core
    CommonCore
    CommonDataModel
    CommonExecutionModel
    CommonMath
    CommonTransforms

    # IO
    IOCore
    IOImage
    IOXML

    # Filtering
    FiltersCore
    FiltersGeneral
    FiltersGeometry
    FiltersModeling
    FiltersSources

    # Imaging
    ImagingCore
    ImagingGeneral
    ImagingMath

    # Rendering
    RenderingCore
    RenderingOpenGL2
    RenderingVolume
    RenderingVolumeOpenGL2

    # Interaction
    InteractionStyle
    InteractionWidgets

    # (Optional) Qt integration
    GUISupportQt
)

add_executable(dicom_viewer main.cpp)
target_link_libraries(dicom_viewer PRIVATE ${VTK_LIBRARIES})
vtk_module_autoinit(TARGETS dicom_viewer MODULES ${VTK_LIBRARIES})
```

---

## 11. References

### Official Documentation
- [VTK Documentation](https://vtk.org/documentation/)
- [VTK Examples](https://examples.vtk.org/)
- [VTK Doxygen API](https://vtk.org/doc/nightly/html/)

### Medical Imaging Specific
- [Kitware VTK for Medical Imaging Training](https://www.kitware.eu/vtk-for-medical-imaging-training/)
- [3D Slicer Developer Guide](https://slicer.readthedocs.io/)

### Related Libraries
- [vtk-dicom](https://github.com/dgobbi/vtk-dicom) - Enhanced DICOM support
- [GDCM](https://gdcm.sourceforge.net/) - Grassroots DICOM

---

*Previous document: [01-itk-overview.md](01-itk-overview.md) - ITK Overview*
*Next document: [03-itk-vtk-integration.md](03-itk-vtk-integration.md) - ITK-VTK Integration Guide*

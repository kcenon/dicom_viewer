# DICOM Viewer Reference Documentation

> **Version**: 1.0.0
> **Last Updated**: 2025-12-31
> **Technology Stack**: ITK, VTK, pacs_system, Qt6

## Overview

This document provides foundational knowledge for implementing MRI/CT medical image viewers using ITK and VTK.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    DICOM Viewer Architecture                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ┌─────────────┐    ┌─────────────┐    ┌─────────────────────────────┐│
│   │ pacs_system │ →  │     ITK     │ →  │           VTK               ││
│   │             │    │             │    │                             ││
│   │ DICOM I/O   │    │ Processing  │    │ Visualization               ││
│   │ Network     │    │ Filtering   │    │ Volume Rendering            ││
│   │ Storage     │    │ Segmentation│    │ Surface Rendering           ││
│   │             │    │ Registration│    │ MPR Views                   ││
│   └─────────────┘    └─────────────┘    └─────────────────────────────┘│
│         │                   │                        │                 │
│         └───────────────────┴────────────────────────┘                 │
│                              │                                         │
│                     ┌────────┴────────┐                                │
│                     │      Qt6        │                                │
│                     │   Application   │                                │
│                     └─────────────────┘                                │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Document Index

### Core Technology References

| # | Document | Description | Key Topics |
|---|----------|-------------|------------|
| 01 | [ITK Overview](01-itk-overview.md) | ITK Architecture and API | Dataflow Pipeline, Segmentation, Registration, DICOM Processing |
| 02 | [VTK Overview](02-vtk-overview.md) | VTK Architecture and API | Volume Rendering, Surface Rendering, MPR, Qt Integration |
| 03 | [ITK-VTK Integration](03-itk-vtk-integration.md) | ITK-VTK Integration Guide | Data Conversion, Coordinate Systems, Integrated Pipeline |
| 04 | [DICOM Pipeline](04-dicom-pipeline.md) | Medical Image Processing Pipeline | Preprocessing, Segmentation, Registration, Measurement, Surface Extraction |
| 05 | [pacs_system Integration](05-pacs-integration.md) | pacs_system Integration | DICOM File Processing, Network Services, Conversion |
| 06 | [GUI Framework Comparison](06-gui-framework-comparison.md) | C++ GUI Framework Analysis | Qt, Dear ImGui, wxWidgets, GTK, FLTK, VTK Integration |
| 07 | [Remote Visualization](07-remote-visualization.md) | Server-Side Rendering Architecture | VTK Streaming, WebSocket, Adaptive Quality, Platform-Independent Viewing |

---

## Quick Start Guide

### 1. Environment Setup

```bash
# Install required dependencies (macOS)
brew install itk vtk qt@6

# Using vcpkg
vcpkg install itk[vtk] vtk[qt] qt6
```

### 2. Basic Structure

```cpp
// Basic DICOM Viewer Flow
#include <pacs/core/dicom_file.hpp>
#include <itkImage.h>
#include <itkImageToVTKImageFilter.h>
#include <vtkRenderer.h>

// 1. Read DICOM with pacs_system
auto pacsFile = pacs::dicom_file::open("image.dcm").value();

// 2. Convert and process with ITK
auto itkImage = pacsToITK3D<short>(dicomDirectory);
auto filtered = ApplyGaussianFilter(itkImage, sigma);

// 3. Convert and visualize with VTK
auto connector = itk::ImageToVTKImageFilter<ImageType>::New();
connector->SetInput(filtered);
volumeMapper->SetInputData(connector->GetOutput());
```

### 3. Key Class Mapping

| Role | pacs_system | ITK | VTK |
|------|-------------|-----|-----|
| **Data Storage** | `dicom_dataset` | `itk::Image<T,D>` | `vtkImageData` |
| **File I/O** | `dicom_file` | `itk::ImageSeriesReader` | `vtkDICOMImageReader` |
| **Filtering** | - | `itk::ImageToImageFilter` | `vtkImageAlgorithm` |
| **Volume Rendering** | - | - | `vtkGPUVolumeRayCastMapper` |
| **Surface Extraction** | - | - | `vtkMarchingCubes` |

---

## Technology Comparison

### ITK vs VTK Role Division

```
┌─────────────────────────────────────────────────────────────────┐
│           When to Use ITK vs VTK                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   Use ITK for:                  Use VTK for:                    │
│   ────────────                  ────────────                    │
│   ✓ DICOM Reading               ✓ 3D Rendering                  │
│   ✓ Image Filtering             ✓ Volume Rendering              │
│   ✓ Segmentation                ✓ Surface Rendering             │
│   ✓ Registration                ✓ MPR Visualization             │
│   ✓ Spatial Transforms          ✓ Interactive Widgets           │
│   ✓ Medical-specific            ✓ GUI Integration (Qt)          │
│     algorithms                  ✓ Mesh Processing               │
│                                                                 │
│   Use pacs_system for:                                          │
│   ───────────────────                                           │
│   ✓ DICOM File Handling (alternative to ITK GDCM)              │
│   ✓ Network Services (C-FIND, C-MOVE, C-STORE)                 │
│   ✓ PACS Integration                                            │
│   ✓ Transfer Syntax / Compression Handling                      │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Key Concepts

### Coordinate Systems

| System | Usage | X+ | Y+ | Z+ |
|--------|-------|----|----|-----|
| **LPS** | DICOM, ITK | Left | Posterior | Superior |
| **RAS** | 3D Slicer | Right | Anterior | Superior |
| **Image** | VTK | Right | Down | Into |

### Hounsfield Units (CT)

| Tissue | HU Range |
|--------|----------|
| Air | -1000 |
| Lung | -500 ~ -900 |
| Fat | -100 ~ -50 |
| Water | 0 |
| Muscle | 10 ~ 40 |
| Bone | 300 ~ 3000 |

---

## CMake Template

```cmake
cmake_minimum_required(VERSION 3.20)
project(dicom_viewer)

set(CMAKE_CXX_STANDARD 20)

# Find packages
find_package(pacs_system REQUIRED)
find_package(ITK REQUIRED COMPONENTS ITKCommon ITKIOImageBase ITKIOGDCM ITKVtkGlue)
find_package(VTK REQUIRED COMPONENTS CommonCore RenderingCore RenderingVolumeOpenGL2 GUISupportQt)
find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets)

include(${ITK_USE_FILE})

add_executable(dicom_viewer main.cpp)

target_link_libraries(dicom_viewer PRIVATE
    pacs::pacs_core
    pacs::pacs_encoding
    ${ITK_LIBRARIES}
    ${VTK_LIBRARIES}
    Qt6::Widgets
    Qt6::OpenGLWidgets
)

vtk_module_autoinit(TARGETS dicom_viewer MODULES ${VTK_LIBRARIES})
```

---

## External Resources

### Official Documentation

- **ITK**: [https://itk.org/](https://itk.org/)
  - [Software Guide](https://itk.org/ITKSoftwareGuide/html/)
  - [Examples](https://examples.itk.org/)
  - [API Reference](https://docs.itk.org/projects/doxygen/)

- **VTK**: [https://vtk.org/](https://vtk.org/)
  - [Documentation](https://vtk.org/documentation/)
  - [Examples](https://examples.vtk.org/)
  - [VTK in Action](https://vtk.org/vtk-in-action/)

- **DICOM Standard**: [https://dicom.nema.org/](https://dicom.nema.org/)
  - [Standard Browser](https://dicom.innolitics.com/)

### Related Projects

- [3D Slicer](https://www.slicer.org/) - Medical Image Analysis Platform
- [MITK](https://www.mitk.org/) - ITK+VTK Integration Framework
- [ParaView](https://www.paraview.org/) - Scientific Data Visualization
- [vtk-dicom](https://github.com/dgobbi/vtk-dicom) - Enhanced VTK DICOM Support

### Research Papers

- Ibanez, L. et al. "The ITK Software Guide"
- Schroeder, W. et al. "The Visualization Toolkit: An Object-Oriented Approach to 3D Graphics"
- [PMC2039808](https://pmc.ncbi.nlm.nih.gov/articles/PMC2039808/) - "Rapid Development of Medical Imaging Tools"

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2025-12-31 | Initial documentation |

---

## Contributing

If you have suggestions for improvements or find errors, please report them through Issues.

---

*For pacs_system specific documentation, see: `/Users/dongcheolshin/Sources/pacs_system/docs/`*

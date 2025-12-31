# DICOM Viewer Reference Documentation

> **Version**: 1.0.0
> **Last Updated**: 2025-12-31
> **Technology Stack**: ITK, VTK, pacs_system, Qt6

## Overview

이 문서는 ITK와 VTK를 이용한 MRI/CT 의료 영상 Viewer 구현을 위한 기반 지식을 제공합니다.

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
| 01 | [ITK Overview](01-itk-overview.md) | ITK 아키텍처 및 API | 데이터플로우 파이프라인, 분할, 정합, DICOM 처리 |
| 02 | [VTK Overview](02-vtk-overview.md) | VTK 아키텍처 및 API | 볼륨 렌더링, 표면 렌더링, MPR, Qt 통합 |
| 03 | [ITK-VTK Integration](03-itk-vtk-integration.md) | ITK-VTK 통합 가이드 | 데이터 변환, 좌표계, 통합 파이프라인 |
| 04 | [DICOM Pipeline](04-dicom-pipeline.md) | 의료영상 처리 파이프라인 | 전처리, 분할, 정합, 측정, 표면 추출 |
| 05 | [pacs_system Integration](05-pacs-integration.md) | pacs_system 연동 | DICOM 파일 처리, 네트워크 서비스, 변환 |
| 06 | [GUI Framework Comparison](06-gui-framework-comparison.md) | C++ GUI 프레임워크 비교 분석 | Qt, Dear ImGui, wxWidgets, GTK, FLTK, VTK 통합 |
| 07 | [Remote Visualization](07-remote-visualization.kr.md) | 서버 사이드 렌더링 아키텍처 | VTK 스트리밍, WebSocket, 적응형 품질, 플랫폼 독립 뷰잉 |

---

## Quick Start Guide

### 1. 환경 설정

```bash
# 필수 의존성 설치 (macOS)
brew install itk vtk qt@6

# vcpkg 사용 시
vcpkg install itk[vtk] vtk[qt] qt6
```

### 2. 기본 구조

```cpp
// 기본 DICOM Viewer 흐름
#include <pacs/core/dicom_file.hpp>
#include <itkImage.h>
#include <itkImageToVTKImageFilter.h>
#include <vtkRenderer.h>

// 1. pacs_system으로 DICOM 읽기
auto pacsFile = pacs::dicom_file::open("image.dcm").value();

// 2. ITK로 변환 및 처리
auto itkImage = pacsToITK3D<short>(dicomDirectory);
auto filtered = ApplyGaussianFilter(itkImage, sigma);

// 3. VTK로 변환 및 시각화
auto connector = itk::ImageToVTKImageFilter<ImageType>::New();
connector->SetInput(filtered);
volumeMapper->SetInputData(connector->GetOutput());
```

### 3. 주요 클래스 매핑

| 역할 | pacs_system | ITK | VTK |
|------|-------------|-----|-----|
| **데이터 저장** | `dicom_dataset` | `itk::Image<T,D>` | `vtkImageData` |
| **파일 I/O** | `dicom_file` | `itk::ImageSeriesReader` | `vtkDICOMImageReader` |
| **필터링** | - | `itk::ImageToImageFilter` | `vtkImageAlgorithm` |
| **볼륨 렌더링** | - | - | `vtkGPUVolumeRayCastMapper` |
| **표면 추출** | - | - | `vtkMarchingCubes` |

---

## Technology Comparison

### ITK vs VTK 역할 분담

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

### 좌표계 (Coordinate Systems)

| 좌표계 | 사용처 | X+ | Y+ | Z+ |
|--------|--------|----|----|-----|
| **LPS** | DICOM, ITK | Left | Posterior | Superior |
| **RAS** | 3D Slicer | Right | Anterior | Superior |
| **Image** | VTK | Right | Down | Into |

### Hounsfield Units (CT)

| 조직 | HU 범위 |
|------|---------|
| 공기 | -1000 |
| 폐 | -500 ~ -900 |
| 지방 | -100 ~ -50 |
| 물 | 0 |
| 근육 | 10 ~ 40 |
| 뼈 | 300 ~ 3000 |

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

- [3D Slicer](https://www.slicer.org/) - 의료 영상 분석 플랫폼
- [MITK](https://www.mitk.org/) - ITK+VTK 통합 프레임워크
- [ParaView](https://www.paraview.org/) - 과학 데이터 시각화
- [vtk-dicom](https://github.com/dgobbi/vtk-dicom) - 향상된 VTK DICOM 지원

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

문서 개선 제안이나 오류 발견 시 Issues를 통해 알려주세요.

---

*For pacs_system specific documentation, see: `/Users/dongcheolshin/Sources/pacs_system/docs/`*

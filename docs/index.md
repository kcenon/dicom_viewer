---
layout: default
title: Home
nav_order: 1
---

# DICOM Viewer Documentation

[![CI](https://github.com/kcenon/dicom_viewer/actions/workflows/ci.yml/badge.svg)](https://github.com/kcenon/dicom_viewer/actions/workflows/ci.yml)
[![Pages](https://github.com/kcenon/dicom_viewer/actions/workflows/pages.yml/badge.svg)](https://github.com/kcenon/dicom_viewer/actions/workflows/pages.yml)

A high-performance medical image viewer designed for radiologists and medical imaging researchers. Primarily supports 3D volume rendering and MPR (Multi-Planar Reconstruction) views for CT and MRI images, with basic 2D viewing for DR/CR images.

## Key Features

- **CT/MRI 3D Visualization**: GPU-accelerated volume rendering, surface rendering, MPR views
- **Region Segmentation**: Automatic (Otsu, Region Growing), semi-automatic (Level Set, Watershed), and manual tools
- **Measurement & Analysis**: Linear, area, volume measurement with ROI statistics and report generation
- **4D Flow MRI Analysis**: Velocity field assembly, streamlines, hemodynamic parameters (WSS, OSI, TKE)
- **Cardiac CT Analysis**: ECG-gated phases, coronary CTA centerline, calcium scoring
- **Enhanced DICOM IOD**: Multi-frame Enhanced CT/MR parsing with per-frame functional groups
- **PACS Integration**: C-ECHO, C-FIND, C-MOVE, C-STORE via [pacs_system](https://github.com/kcenon/pacs_system)
- **Data Export**: NRRD, DICOM, STL/OBJ/PLY mesh, PDF/HTML reports, video export

## Documentation

| Document | Description |
|----------|-------------|
| [PRD](PRD.md) | Product Requirements Document - Vision, Goals, Feature Requirements |
| [SRS](SRS.md) | Software Requirements Specification - Detailed Technical Requirements |
| [SDS](SDS.md) | Software Design Specification - Architecture and Module Design |

## Reference Documentation

Technical reference documents covering the core technology stack:

| # | Document | Description |
|---|----------|-------------|
| 01 | [ITK Overview](reference/01-itk-overview.md) | ITK Architecture and API |
| 02 | [VTK Overview](reference/02-vtk-overview.md) | VTK Architecture and API |
| 03 | [ITK-VTK Integration](reference/03-itk-vtk-integration.md) | ITK-VTK Integration Guide |
| 04 | [DICOM Pipeline](reference/04-dicom-pipeline.md) | Medical Image Processing Pipeline |
| 05 | [pacs_system Integration](reference/05-pacs-integration.md) | pacs_system Integration Guide |
| 06 | [GUI Framework Comparison](reference/06-gui-framework-comparison.md) | C++ GUI Framework Analysis |
| 07 | [Remote Visualization](reference/07-remote-visualization.md) | Server-Side Rendering Architecture |

## Korean Documentation (한국어)

| 문서 | 설명 |
|------|------|
| [PRD (한국어)](PRD.kr.md) | 제품 요구사항 정의서 |
| [SRS (한국어)](SRS.kr.md) | 소프트웨어 요구사항 명세서 |
| [SDS (한국어)](SDS.kr.md) | 소프트웨어 설계 명세서 |
| [Reference Index (한국어)](reference/README.kr.md) | 기술 참조 문서 색인 |

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│  Presentation Layer (Qt6)                                               │
│  • MainWindow, ViewportWidget, PatientBrowser, ToolsPanel               │
│  • SegmentationPanel, StatisticsPanel, FlowToolPanel, ReportPanel       │
├─────────────────────────────────────────────────────────────────────────┤
│  Service Layer                                                          │
│  • Render: VolumeRenderer, SurfaceRenderer, MPRRenderer (VTK)           │
│  • Segmentation: Threshold, RegionGrowing, LevelSet, Watershed (ITK)    │
│  • Measurement: Linear, Area, Volume, ROI Statistics                    │
│  • PACS: DicomFind/Move/Store/Echo (pacs_system)                        │
│  • Flow: FlowDicomParser, VelocityField, FlowVisualizer                │
│  • Cardiac: PhaseDetector, CenterlineExtractor, CalciumScorer           │
│  • Export: Report, MeshExporter, DicomSR, VideoExporter                 │
├─────────────────────────────────────────────────────────────────────────┤
│  Core / Data Layer                                                      │
│  • DicomLoader, SeriesBuilder, TransferSyntaxDecoder                    │
│  • HounsfieldConverter, ImageConverter (ITK↔VTK)                        │
│  • ImageData (ITK/VTK), DicomData (pacs_system), SegmentData            │
└─────────────────────────────────────────────────────────────────────────┘
```

## Technology Stack

| Component | Technology | Version |
|-----------|------------|---------|
| **Language** | C++ | C++23 |
| **Build** | CMake | 3.20+ |
| **GUI** | Qt | 6.5+ |
| **Image Processing** | ITK | 5.4+ |
| **Visualization** | VTK | 9.3+ |
| **DICOM Network** | pacs_system | Latest |

## Roadmap

| Version | Goal | Key Features | Status |
|---------|------|--------------|--------|
| v0.3 (MVP) | CT/MRI Viewer | DICOM loading, Volume/Surface rendering, MPR, Presets | Complete |
| v0.4 | Core Features | PACS integration, Measurement tools, Basic segmentation | Complete |
| v0.5 | Enhancement | DR/CR viewing, Advanced segmentation, Report generation | Complete |
| v0.6 | 4D Flow MRI | Flow DICOM parsing, Velocity field, Streamlines, Hemodynamics | Complete |
| v0.7 | Cardiac & Export | Enhanced DICOM, Cardiac CT, Cine MRI, Data export | Complete |
| v1.0 | Release | Stabilization, Performance optimization, User documentation | Next |

## Quick Start

### Prerequisites

- C++23 compatible compiler (GCC 13+, Clang 16+, MSVC 2022+)
- CMake 3.20+
- Qt 6.5+, ITK 5.4+, VTK 9.3+

### Build

```bash
# Install dependencies (macOS)
brew install itk vtk qt@6 fftw spdlog fmt nlohmann-json

# Build pacs_system first (sibling directory)
cmake -S ../pacs_system -B ../pacs_system/build -DCMAKE_BUILD_TYPE=Release
cmake --build ../pacs_system/build --parallel

# Build dicom_viewer
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## License

BSD 3-Clause License - See [LICENSE](https://github.com/kcenon/dicom_viewer/blob/main/LICENSE) for details.

---

[View on GitHub](https://github.com/kcenon/dicom_viewer) | [Report Issues](https://github.com/kcenon/dicom_viewer/issues)

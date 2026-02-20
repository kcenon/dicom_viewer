# DICOM Viewer

> **High-Performance Medical Image Viewer** - CT/MRI 3D Volume Rendering and MPR View Support

[![Version](https://img.shields.io/badge/version-0.7.0--pre-orange)](https://github.com)
[![C++](https://img.shields.io/badge/C++-23-blue.svg)](https://isocpp.org)
[![License](https://img.shields.io/badge/license-BSD--3--Clause-green.svg)](LICENSE)

## Overview

**DICOM Viewer** is a high-performance medical image viewer designed for radiologists and medical imaging researchers. It primarily supports 3D volume rendering and MPR (Multi-Planar Reconstruction) views for CT and MRI images, while also providing basic 2D viewing capabilities for DR/CR images.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          DICOM Viewer                                    │
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

## Key Features

### CT/MRI 3D Visualization (P0 - Critical)

| Feature | Description |
|---------|-------------|
| **Volume Rendering** | GPU-accelerated Ray Casting, Transfer Function editing, Presets (CT Bone, Soft Tissue, Lung, Angio) |
| **MPR View** | Simultaneous Axial, Coronal, Sagittal display, Crosshair synchronization, Oblique reslicing |
| **Surface Rendering** | Marching Cubes isosurface extraction, Multi-surface simultaneous rendering, STL/PLY export |
| **Volume Clipping** | Interactive clipping box for internal structure examination |

### Region Segmentation (P1 - High)

| Method | Description |
|--------|-------------|
| **Automatic Segmentation** | Otsu thresholding, Region Growing, Confidence Connected |
| **Semi-automatic Segmentation** | Level Set (Geodesic Active Contour), Watershed |
| **Manual Segmentation** | Brush, Eraser, Fill, Polygon, Freehand curve, Smart Scissors (LiveWire) |
| **Post-processing** | Opening, Closing, Fill Holes, Island Removal |

### Region Measurement and Analysis (P1 - High)

- **Linear Measurement**: Distance (2 points), Angle (3 points), Cobb angle
- **Area Measurement**: Circular/Elliptical, Polygon, Freehand ROI
- **Volume Measurement**: Segmented region volume calculation (mm³, cm³, mL)
- **ROI Statistics**: Mean, StdDev, Min/Max, Histogram
- **Advanced Analysis**: Surface area, Sphericity, Center of mass

### PACS Integration (P1 - High)

| Service | Functionality |
|---------|---------------|
| **C-ECHO** | Connection test |
| **C-FIND** | Patient/Study/Series/Image level search |
| **C-MOVE** | Image retrieval |
| **C-STORE SCP** | Image reception |

### DR/CR 2D Viewing (P2 - Medium)

- Window/Level adjustment (mouse drag)
- Zoom, Pan, Rotate
- Horizontal/Vertical flip
- Grid view (multi-image comparison)

### 4D Flow MRI Analysis (P1 - High)

| Feature | Description |
|---------|-------------|
| **Flow DICOM Parsing** | Vendor-specific (Siemens, Philips, GE) 4D Flow DICOM parsing with VENC extraction |
| **Velocity Field Assembly** | Vector field construction from velocity-encoded components with VENC scaling |
| **Phase Correction** | Velocity aliasing unwrap, eddy current correction, Maxwell term correction |
| **Flow Visualization** | Streamlines, pathlines, vector glyphs with color-mapped hemodynamic parameters |
| **Flow Quantification** | Flow rate at cross-section, time-velocity curves, pressure gradient estimation |
| **Hemodynamic Analysis** | Wall Shear Stress (WSS), Oscillatory Shear Index (OSI), Turbulent Kinetic Energy (TKE) |

### Enhanced DICOM Support (P0 - Critical)

| Feature | Description |
|---------|-------------|
| **Enhanced CT/MR IOD** | Multi-frame Enhanced DICOM parsing with per-frame functional groups |
| **Frame Extraction** | Individual frame extraction from multi-frame pixel data |
| **Series Classification** | Automatic classification of Enhanced series by type |

### Cardiac CT Analysis (P1 - High)

| Feature | Description |
|---------|-------------|
| **ECG-Gated Phases** | Cardiac phase detection and separation from ECG-gated CT |
| **Coronary CTA** | Centerline extraction, CPR (Curved Planar Reformation), stenosis measurement |
| **Calcium Scoring** | Agatston, volume, and mass calcium scores with per-artery breakdown |
| **Cine MRI** | Multi-phase cardiac cine playback with orientation detection (SA, 2CH, 3CH, 4CH) |

### Data Export and Interoperability (P1 - High)

| Feature | Description |
|---------|-------------|
| **Image Export** | NRRD and DICOM format export with metadata preservation |
| **Mesh Export** | STL (binary/ASCII), OBJ, PLY 3D mesh export |
| **Measurement Export** | JSON, CSV serialization and DICOM Structured Report |
| **Video Export** | AVI/MP4/MOV generation with configurable FPS and codec |
| **Research Export** | MATLAB .mat and Ensight Gold format for external analysis |
| **Report Generation** | PDF/HTML medical imaging reports with templates |

### Project Management

| Feature | Description |
|---------|-------------|
| **Project Files** | .flo project file format (ZIP-based container) |
| **Save/Load** | Persist and restore display settings, segmentation, analysis results |
| **Drag & Drop** | Drag-and-drop DICOM and project file import |

## Technology Stack

| Component | Technology | Version |
|-----------|------------|---------|
| **Language** | C++ | C++23 |
| **Build** | CMake | 3.20+ |
| **GUI** | Qt | 6.5+ |
| **Image Processing** | ITK | 5.4+ |
| **Visualization** | VTK | 9.3+ |
| **DICOM Network** | pacs_system | Latest |
| **DICOM I/O** | GDCM (via ITK) | Latest |

> **Note**: DICOM network operations use [pacs_system](https://github.com/kcenon/pacs_system), a pure C++20 PACS implementation from the kcenon ecosystem.

## Architecture

DICOM Viewer adopts a **3-Layer Architecture** to maximize separation of concerns and maintainability. UI components access service classes directly without an intermediary controller layer.

```
┌─────────────────────────────────────────────────────────────────────────┐
│  Presentation Layer (Qt6)                                               │
│  • MainWindow, ViewportWidget, PatientBrowser, ToolsPanel               │
│  • SegmentationPanel, StatisticsPanel, FlowToolPanel, ReportPanel       │
│  • SettingsDialog, PacsConfigDialog, QuantificationWindow               │
├─────────────────────────────────────────────────────────────────────────┤
│  Service Layer                                                          │
│  • Render: VolumeRenderer, SurfaceRenderer, MPRRenderer (VTK)           │
│  • Segmentation: Threshold, RegionGrowing, LevelSet, Watershed (ITK)    │
│  • Measurement: Linear, Area, Volume, ROI Statistics, ShapeAnalyzer     │
│  • Preprocessing: Gaussian, AnisotropicDiffusion, N4Bias, Resampler     │
│  • PACS: DicomFind/Move/Store/Echo (pacs_system)                        │
│  • Flow: FlowDicomParser, VelocityField, FlowVisualizer, Quantifier    │
│  • Export: Report, MeshExporter, DicomSR, DataExporter, VideoExporter   │
│  • Cardiac: PhaseDetector, CenterlineExtractor, CalciumScorer           │
│  • Enhanced DICOM: EnhancedParser, FrameExtractor, SeriesClassifier     │
├─────────────────────────────────────────────────────────────────────────┤
│  Core / Data Layer                                                      │
│  • DicomLoader, SeriesBuilder, TransferSyntaxDecoder                    │
│  • HounsfieldConverter, ImageConverter (ITK↔VTK)                        │
│  • ImageData (ITK/VTK), DicomData (pacs_system), SegmentData            │
└─────────────────────────────────────────────────────────────────────────┘
```

## Performance Goals

| Metric | Target | Condition |
|--------|--------|-----------|
| CT Series Loading | ≤ 3 sec | 512×512×300 volume |
| Volume Rendering FPS | ≥ 30 FPS | Interactive rotation/zoom |
| MPR Slice Switching | ≤ 100ms | Mouse wheel scroll |
| Memory Usage | ≤ 2GB | Based on 1GB volume data |
| Application Startup | ≤ 5 sec | Cold Start |

## Supported Environments

### Operating Systems

- macOS 12+ (Monterey)
- Windows 10+ (64-bit)
- Ubuntu 20.04 LTS+

### Hardware Requirements

- **GPU**: OpenGL 4.1+ support (VRAM 2GB+, 4GB recommended)
- **RAM**: 8GB+ (16GB recommended)
- **Storage**: 1GB+ (for installation)

### Supported DICOM Formats

| Transfer Syntax | Support |
|-----------------|---------|
| Implicit VR Little Endian | ✅ |
| Explicit VR Little Endian | ✅ |
| JPEG Baseline | ✅ |
| JPEG Lossless | ✅ |
| JPEG 2000 | ✅ |
| JPEG-LS | ✅ |
| RLE Lossless | ✅ |

## Getting Started

### Install Dependencies

```bash
# macOS (Homebrew)
brew install itk vtk qt@6 fftw spdlog fmt nlohmann-json

# Using vcpkg
vcpkg install itk[vtk] vtk[qt] qt6 fftw3 spdlog fmt nlohmann-json
```

### Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_TESTS` | ON | Build unit tests |
| `BUILD_DOCS` | OFF | Build documentation |

### pacs_system Integration (Required)

The project requires [pacs_system](https://github.com/kcenon/pacs_system) library for DICOM network operations. This is a pure C++20 implementation from the kcenon ecosystem.

```bash
# Build pacs_system first (sibling directory)
cd ../pacs_system
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel

# Build dicom_viewer
cd ../../dicom_viewer
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

Custom pacs_system location:

```bash
cmake .. \
  -DPACS_SYSTEM_ROOT=/path/to/pacs_system \
  -DPACS_SYSTEM_BUILD_DIR=/path/to/pacs_system/build
```

### Run

```bash
./dicom_viewer
```

## Project Structure

```
dicom_viewer/
├── docs/                           # Documentation
│   ├── PRD.md / PRD.kr.md         # Product Requirements Document
│   ├── SRS.md / SRS.kr.md         # Software Requirements Specification
│   ├── SDS.md / SDS.kr.md         # Software Design Specification
│   └── reference/                  # Technical Reference (ITK, VTK, pacs_system)
├── include/                        # Header Files
│   ├── core/                       # Core module headers
│   │   ├── dicom_loader.hpp        # DICOM file loading
│   │   ├── series_builder.hpp      # Series assembly and 3D volume
│   │   ├── transfer_syntax_decoder.hpp  # Transfer syntax decoding
│   │   ├── hounsfield_converter.hpp  # CT pixel to HU conversion
│   │   └── image_converter.hpp     # ITK↔VTK format conversion
│   ├── services/                   # Service layer headers
│   │   ├── render/                 # Oblique reslice renderer
│   │   ├── measurement/            # Linear, area, volume, ROI, shape tools
│   │   ├── preprocessing/          # Gaussian, anisotropic, N4, resampler
│   │   ├── segmentation/           # Threshold, region growing, level set, etc.
│   │   ├── export/                 # Report, mesh, DICOM SR, data export
│   │   ├── coordinate/             # MPR coordinate transformer
│   │   ├── volume_renderer.hpp     # GPU volume rendering with CPU fallback
│   │   ├── surface_renderer.hpp    # Isosurface extraction and rendering
│   │   ├── mpr_renderer.hpp        # MPR (Multi-Planar Reconstruction) views
│   │   └── dicom_*_scu.hpp        # PACS services (Echo, Find, Move, Store)
│   └── ui/                         # UI headers
│       ├── panels/                 # PatientBrowser, ToolsPanel, etc.
│       └── dialogs/                # SettingsDialog, PacsConfigDialog
├── src/                            # Source Code
│   ├── app/                        # Application entry point (main.cpp)
│   ├── core/                       # Core Data Structures
│   │   ├── dicom/                  # DICOM loading and series assembly
│   │   ├── image/                  # Image processing and HU conversion
│   │   └── logging/                # spdlog-based logging
│   ├── services/                   # Service Layer
│   │   ├── render/                 # Volume/Surface/MPR/Oblique rendering
│   │   ├── measurement/            # Measurement and statistics tools
│   │   ├── preprocessing/          # Image filters (Gaussian, N4, etc.)
│   │   ├── segmentation/           # Segmentation algorithms and label mgmt
│   │   ├── pacs/                   # PACS connectivity (C-ECHO, C-FIND, etc.)
│   │   ├── export/                 # Report, mesh, data, video export
│   │   ├── flow/                   # 4D Flow MRI analysis
│   │   ├── enhanced_dicom/         # Enhanced DICOM IOD parsing
│   │   ├── cardiac/                # Cardiac CT/MRI analysis
│   │   └── coordinate/             # MPR coordinate transformations
│   └── ui/                         # Qt UI Components
│       ├── widgets/                # ViewportWidget, MPRViewWidget, DRViewer
│       ├── panels/                 # PatientBrowser, ToolsPanel, etc.
│       └── dialogs/                # Settings, PACS configuration
├── tests/                          # Test Suite
│   ├── unit/                       # Unit tests (40+ test files)
│   ├── integration/                # Integration tests
│   └── data/                       # Test DICOM data
├── LICENSE
└── README.md
```

## Documentation

| Document | Description |
|----------|-------------|
| [PRD](docs/PRD.md) | Product Requirements Document - Vision, Goals, Feature Requirements |
| [SRS](docs/SRS.md) | Software Requirements Specification - Detailed Technical Requirements |
| [SDS](docs/SDS.md) | Software Design Specification - Architecture and Module Design |
| [Reference](docs/reference/README.md) | Technical Reference Documentation - ITK, VTK, pacs_system |

## Roadmap

```
v0.1 → v0.2 → v0.3 (MVP) → v0.4 → v0.5 → v0.6 → v0.7 → v1.0
──────────────────────────────────────────────────────────────────
Phase 1        Phase 2: Core     Phase 3          Phase 4
Foundation     Features          Enhancement      Advanced
```

| Version | Goal | Key Features | Status |
|---------|------|--------------|--------|
| v0.3 (MVP) | CT/MRI Viewer | DICOM loading, Volume/Surface rendering, MPR, Presets | ✅ Complete |
| v0.4 | Core Features | PACS integration, Measurement tools, Basic segmentation, Preprocessing | ✅ Complete |
| v0.5 | Enhancement | DR/CR viewing, Advanced segmentation, Report generation | ✅ Complete |
| v0.6 | 4D Flow MRI | Flow DICOM parsing, Velocity field, Streamlines, Hemodynamic analysis | ✅ Complete |
| v0.7 | Cardiac & Export | Enhanced DICOM, Cardiac CT, Cine MRI, Data export, Project management | ✅ Complete |
| v1.0 | Release | Stabilization, Performance optimization, User documentation | Next target |

## Target Users

| User | Primary Requirements |
|------|---------------------|
| **Radiologists** | CT/MRI interpretation, 3D reconstruction, Measurement/Quantitative analysis |
| **Researchers/Developers** | Algorithm validation, Segmentation research, API accessibility |
| **Radiologic Technologists** | DR/CR verification, PACS integration, Simple operation |

## Related Projects

- [pacs_system](https://github.com) - DICOM Processing and PACS Functionality (kcenon ecosystem)
- [3D Slicer](https://www.slicer.org/) - Medical Image Analysis Platform
- [MITK](https://www.mitk.org/) - ITK+VTK Integration Framework

## References

- [ITK Documentation](https://itk.org/)
- [VTK Documentation](https://vtk.org/)
- [DICOM Standard](https://dicom.nema.org/)
- [Qt6 Documentation](https://doc.qt.io/)

## License

BSD 3-Clause License - See the [LICENSE](LICENSE) file for details.

---

> **Note**: The current version (v0.x.x) is a Pre-release under active development. Official releases will begin from v1.0.0.

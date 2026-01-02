# DICOM Viewer

> **High-Performance Medical Image Viewer** - CT/MRI 3D Volume Rendering and MPR View Support

[![Version](https://img.shields.io/badge/version-0.3.0--pre-orange)](https://github.com)
[![C++](https://img.shields.io/badge/C++-20-blue.svg)](https://isocpp.org)
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

## Technology Stack

| Component | Technology | Version |
|-----------|------------|---------|
| **Language** | C++ | C++23 |
| **Build** | CMake | 3.20+ |
| **GUI** | Qt | 6.5+ |
| **Image Processing** | ITK | 5.4+ |
| **Visualization** | VTK | 9.3+ |
| **DICOM Network** | DCMTK | 3.6.8+ |
| **DICOM I/O** | GDCM (via ITK) | Latest |

## Architecture

DICOM Viewer adopts a **4-Layer Architecture** to maximize separation of concerns and maintainability.

```
┌─────────────────────────────────────────────────────────────────────────┐
│  Presentation Layer (Qt6)                                               │
│  • MainWindow, ViewportWidget, ToolsPanel, PatientBrowser               │
│  • SegmentationPanel, StatisticsPanel                                   │
├─────────────────────────────────────────────────────────────────────────┤
│  Controller Layer                                                       │
│  • ViewerController → Loading, Rendering, Tool, Network Controllers     │
├─────────────────────────────────────────────────────────────────────────┤
│  Service Layer                                                          │
│  • ImageService (ITK) • RenderService (VTK) • NetworkService (pacs)     │
│  • MeasurementService • SegmentationService                             │
├─────────────────────────────────────────────────────────────────────────┤
│  Data Layer                                                             │
│  • ImageData (ITK/VTK) • DicomData (pacs_system) • SegmentData          │
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
brew install itk vtk qt@6 fftw dcmtk spdlog fmt nlohmann-json

# Using vcpkg
vcpkg install itk[vtk] vtk[qt] qt6 fftw3 dcmtk spdlog fmt nlohmann-json
```

### Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

### Run

```bash
./dicom_viewer
```

## Project Structure

```
dicom_viewer/
├── docs/                           # Documentation
│   ├── PRD.md                      # Product Requirements Document
│   ├── SRS.md                      # Software Requirements Specification
│   ├── SDS.md                      # Software Design Specification
│   └── reference/                  # Technical Reference Documentation
├── include/                        # Header Files
│   ├── core/                       # Core module headers
│   │   ├── dicom_loader.hpp        # DICOM file loading
│   │   ├── series_builder.hpp      # Series assembly and 3D volume
│   │   ├── transfer_syntax_decoder.hpp  # Transfer syntax decoding support
│   │   ├── hounsfield_converter.hpp  # CT pixel to HU conversion
│   │   └── image_converter.hpp     # Image format conversion
│   └── services/                   # Service layer headers
│       ├── volume_renderer.hpp     # GPU volume rendering with CPU fallback
│       ├── transfer_function_manager.hpp  # Transfer function preset management
│       ├── mpr_renderer.hpp        # MPR (Multi-Planar Reconstruction) views
│       ├── pacs_config.hpp         # PACS server configuration
│       └── dicom_echo_scu.hpp      # DICOM C-ECHO connectivity test
├── src/                            # Source Code
│   ├── core/                       # Core Data Structures
│   │   ├── dicom/                  # DICOM loading and series assembly
│   │   ├── image/                  # Image processing and HU conversion
│   │   └── data/                   # Patient data management
│   ├── services/                   # Service Layer
│   │   ├── image/                  # Image processing services
│   │   ├── render/                 # Volume/Surface/MPR rendering
│   │   ├── measurement/            # Measurement tools
│   │   ├── segmentation/           # Segmentation algorithms (Threshold, Region Growing)
│   │   └── pacs/                   # PACS connectivity (C-ECHO, C-FIND, etc.)
│   ├── controller/                 # Controller Layer
│   └── ui/                         # Qt UI Components
├── tests/                          # Unit and Integration Tests
│   └── unit/                       # Unit tests
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
v0.1 → v0.2 → v0.3 (MVP) → v0.4 → v0.5 → v1.0
───────────────────────────────────────────────
Phase 1        Phase 2: Core     Phase 3
Foundation     Features          Enhancement
```

| Version | Goal | Key Features |
|---------|------|--------------|
| v0.3 (MVP) | CT/MRI Viewer | DICOM loading, Volume/Surface rendering, MPR, Presets |
| v0.4 | Core Features | PACS integration, Measurement tools, Basic segmentation, Preprocessing |
| v0.5 | Enhancement | DR/CR viewing, Advanced segmentation, Report generation |
| v1.0 | Release | Stabilization, User settings, Layout persistence |

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

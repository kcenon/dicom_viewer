# dicom_viewer Architecture

> **Language:** **English** | [한국어](ARCHITECTURE.kr.md)

## Table of Contents

1. [Overview](#overview)
2. [System Structure](#system-structure)
3. [Core Components](#core-components)
4. [Service Layer](#service-layer)
5. [UI Layer](#ui-layer)
6. [Data Flow](#data-flow)
7. [Design Principles](#design-principles)
8. [Dependencies](#dependencies)
9. [Performance Targets](#performance-targets)

---

## Overview

### Purpose

dicom_viewer is a high-performance medical image viewer for radiologists and researchers. It supports 3D volume rendering, MPR (Multi-Planar Reconstruction), segmentation, 4D Flow MRI analysis, and cardiac imaging workflows.

### Design Goals

- **Performance**: GPU-accelerated volume rendering at interactive frame rates
- **Modularity**: Independent service modules with clear interfaces
- **Extensibility**: Plugin-style vendor parsers and export formats
- **Correctness**: DICOM standard compliance and accurate Hounsfield unit handling

---

## System Structure

### Three-Layer Architecture

```
+-----------------------------------------------------------+
|  PRESENTATION LAYER (Qt6)                                 |
|  MainWindow, Viewports, Panels, Dialogs                   |
+-----------------------------------------------------------+
|  SERVICE LAYER (12 modules)                               |
|  Render, Segmentation, Measurement, Preprocessing,        |
|  PACS, Export, Flow, Cardiac, Enhanced DICOM,             |
|  Coordinate                                               |
+-----------------------------------------------------------+
|  CORE / DATA LAYER                                        |
|  DicomLoader, SeriesBuilder, ImageConverter,               |
|  HounsfieldConverter, ProjectManager, DataSerializer       |
+-----------------------------------------------------------+
```

### Directory Layout

```
dicom_viewer/
├── include/
│   ├── core/                   # Core data structures and loaders
│   │   ├── dicom_loader.hpp
│   │   ├── series_builder.hpp
│   │   ├── image_converter.hpp
│   │   ├── hounsfield_converter.hpp
│   │   ├── transfer_syntax_decoder.hpp
│   │   ├── project_manager.hpp
│   │   └── data_serializer.hpp
│   ├── services/               # Service layer (12 modules)
│   │   ├── render/
│   │   ├── segmentation/
│   │   ├── measurement/
│   │   ├── preprocessing/
│   │   ├── export/
│   │   ├── flow/
│   │   ├── enhanced_dicom/
│   │   ├── cardiac/
│   │   └── coordinate/
│   └── ui/                     # Qt6 UI components
│       ├── panels/
│       ├── dialogs/
│       └── widgets/
├── src/                        # Implementation files (mirrors include/)
│   ├── app/main.cpp
│   ├── core/
│   ├── services/
│   └── ui/
├── tests/
│   ├── unit/
│   ├── integration/
│   └── test_utils/
├── docs/                       # Documentation (PRD, SRS, SDS, Architecture)
└── benchmarks/                 # Performance benchmarks
```

### Dependency Diagram

```
dicom_viewer (executable)
    │
    ├── dicom_viewer_ui ─────── Qt6 (Widgets, OpenGL, Concurrent)
    │   ├── render_service ──── VTK (RenderingVolume, InteractionWidgets)
    │   ├── segmentation_service ── ITK (Segmentation, LevelSets)
    │   ├── measurement_service
    │   ├── preprocessing_service ── ITK (Filtering), FFTW3
    │   ├── pacs_service ────── pacs_system (kcenon ecosystem)
    │   ├── export_service
    │   ├── flow_service
    │   ├── cardiac_service
    │   ├── enhanced_dicom_service ── GDCM
    │   └── coordinate_service
    │
    ├── dicom_viewer_core ───── ITK (IO, GDCM), zlib
    │
    └── kcenon ecosystem
        ├── pacs_system (DICOM network)
        ├── thread_system (thread pool)
        ├── logger_system (logging)
        └── common_system (shared interfaces)
```

---

## Core Components

### 1. DicomLoader

**Role**: DICOM file parsing and series assembly

**Key Classes**:
- `DicomLoader`: File and directory scanning with metadata extraction
- `SeriesBuilder`: Assembles ordered slices into 3D ITK volumes

**Responsibilities**:
- Parse DICOM tags (patient info, study/series metadata, pixel data)
- Sort slices by position and orientation
- Validate series consistency (spacing, orientation)
- Build `itk::Image<short, 3>` volumes from ordered slices

### 2. ImageConverter

**Role**: Bridge between ITK and VTK image representations

**Key Operations**:
- `itk::Image` to `vtkImageData` conversion
- Pixel type handling (8-bit, 16-bit, float)
- Coordinate system alignment (ITK LPS to VTK RAS)

### 3. HounsfieldConverter

**Role**: CT pixel value to Hounsfield Unit mapping

**Features**:
- Rescale slope/intercept application
- Tissue type classification (air, fat, water, soft tissue, bone)
- Inverse conversion for stored value lookup

### 4. TransferSyntaxDecoder

**Role**: DICOM transfer syntax support

**Supported Syntaxes**:
- Implicit/Explicit VR Little Endian
- JPEG Baseline, JPEG Lossless (Processes 14, 18)
- JPEG 2000 (Lossless/Lossy)
- JPEG-LS (Lossless/Near-Lossless)
- RLE Lossless

### 5. ProjectManager

**Role**: Project file persistence (.flo format)

**Features**:
- ZIP-based container with NRRD image data
- Segmentation mask serialization
- Measurement and annotation persistence

---

## Service Layer

Each service is built as a separate CMake static library, enabling independent testing and clear dependency boundaries.

### Render Service

6 rendering components providing VTK-based visualization.

| Component | Description |
|-----------|-------------|
| `VolumeRenderer` | GPU ray casting with CPU fallback, transfer function presets |
| `SurfaceRenderer` | Marching Cubes isosurface extraction |
| `MPRRenderer` | Synchronized Axial/Coronal/Sagittal slice views |
| `ObliquResliceRenderer` | Arbitrary-angle reslicing |
| `HemodynamicOverlayRenderer` | 4D Flow overlay on volume |
| `StreamlineOverlayRenderer` | Fiber tracking visualization |
| `TransferFunctionManager` | Color/opacity preset management with custom preset support |

### Segmentation Service

16 components for interactive and automated segmentation.

| Component | Description |
|-----------|-------------|
| `ThresholdSegmenter` | Otsu automatic thresholding |
| `RegionGrowingSegmenter` | Confidence connected region growing |
| `LevelSetSegmenter` | Geodesic active contour (GAC) |
| `WatershedSegmenter` | Morphological watershed |
| `ManualSegmentationController` | Brush/eraser/fill tools |
| `LabelManager` | Multi-label organization and color assignment |
| `MorphologicalProcessor` | Opening, closing, island removal |
| `SliceInterpolator` | Gap filling between manual segmentations |
| `CenterlineTracer` | Vessel centerline extraction |
| `LevelTracingTool` | Contour tracing at a specific intensity level |
| `HollowTool` | Hollow-out segmented structures |
| `MaskSmoother` | Gaussian smoothing on label maps |
| `PhaseTracker` | Temporal mask propagation across cardiac phases |
| `SegmentationCommand` | Undo/redo framework (Command pattern) |
| `SnapshotCommand` | RLE-compressed undo history |
| `LabelMapOverlay` | Segmentation overlay visualization |

### Measurement Service

| Component | Description |
|-----------|-------------|
| `LinearMeasurementTool` | Distance and angle measurements |
| `AreaMeasurementTool` | Circular, elliptical, freehand ROI area |
| `VolumeCalculator` | Segmented region volume (mm3, cm3, mL) |
| `ROIStatistics` | Mean, StdDev, Min/Max, histogram |
| `ShapeAnalyzer` | Surface area, sphericity, center of mass |

### Preprocessing Service

| Component | Description |
|-----------|-------------|
| `GaussianSmoother` | Noise reduction via Gaussian blur |
| `AnisotropicDiffusionFilter` | Edge-preserving smoothing |
| `N4BiasCorrector` | MRI B1 bias field correction (requires FFTW) |
| `IsotropicResampler` | Voxel spacing normalization |
| `HistogramEqualizer` | Contrast enhancement |

### PACS Service

Built on the kcenon pacs_system library for DICOM network operations.

| Component | Description |
|-----------|-------------|
| `DicomEchoScu` | C-ECHO verification |
| `DicomFindScu` | C-FIND query (patient/study/series/image) |
| `DicomMoveScu` | C-MOVE image retrieval |
| `DicomStoreScp` | C-STORE SCP server for receiving images |
| `PacsConfigManager` | Server configuration persistence (QSettings) |

### Export Service

| Component | Description |
|-----------|-------------|
| `DataExporter` | NRRD image export with metadata |
| `MeshExporter` | STL (binary/ASCII), OBJ, PLY mesh export |
| `ReportGenerator` | PDF/HTML medical reports |
| `MeasurementSerializer` | JSON/CSV measurement data |
| `DicomSRWriter` | DICOM Structured Report creation |
| `EnsightExporter` | Ensight Gold format for CFD tools |
| `MatlabExporter` | MATLAB v5 .mat file format |
| `VideoExporter` | OGG Theora video generation |

### Flow Service

4D Flow MRI analysis with vendor-specific parsing.

| Component | Description |
|-----------|-------------|
| `FlowDicomParser` | 4D Flow series detection and organization |
| `VelocityFieldAssembler` | Vector field from velocity-encoded components |
| `PhaseCorrector` | Aliasing unwrap and eddy current correction |
| `TemporalNavigator` | Phase/frame navigation for 4D sequences |
| `FlowVisualizer` | Streamline, pathline, glyph rendering |
| `FlowQuantifier` | Flow rate, velocity curves, pressure gradient |
| `VesselAnalyzer` | WSS, OSI, Turbulent Kinetic Energy |

**Vendor Parsers** (Strategy pattern):
- `SiemensFlowParser`: Siemens VENC extraction
- `PhilipsFlowParser`: Philips VENC extraction
- `GEFlowParser`: GE VENC extraction

### Enhanced DICOM Service

Multi-frame Enhanced DICOM IOD parsing using GDCM.

| Component | Description |
|-----------|-------------|
| `EnhancedDicomParser` | Multi-frame IOD detection and parsing |
| `FunctionalGroupParser` | Shared and per-frame metadata extraction |
| `FrameExtractor` | Individual frame extraction from pixel data |
| `DimensionIndexSorter` | Frame ordering by spatial/temporal dimensions |
| `SeriesClassifier` | Single-frame vs multi-frame classification |

### Cardiac Service

| Component | Description |
|-----------|-------------|
| `CardiacPhaseDetector` | ECG-gated phase separation |
| `CalciumScorer` | Agatston, volume, and mass calcium scoring |
| `CoronaryCenterlineExtractor` | Vessel centerline tracing |
| `CurvedPlanarReformatter` | Curved planar reformation (CPR) |
| `CineOrganizer` | Cine sequence organization (SA, 2CH, 3CH, 4CH) |

### Coordinate Service

| Component | Description |
|-----------|-------------|
| `MPRCoordinateTransformer` | Unified coordinate system with crosshair sync |

---

## UI Layer

### Component Hierarchy

```
MainWindow
├── Central Widget
│   ├── ViewportWidget (1x1 or 2x2 layout)
│   │   ├── MPRViewWidget (Axial / Coronal / Sagittal)
│   │   ├── DRViewer (2D DR/CR viewing)
│   │   └── Display3DController (Volume render controls)
│   └── ViewportLayoutManager (layout switching)
│
├── Dock Panels
│   ├── PatientBrowser (series list with classification)
│   ├── ToolsPanel (measurement and segmentation tools)
│   ├── SegmentationPanel (label management, brush controls)
│   ├── StatisticsPanel (ROI statistics display)
│   ├── OverlayControlPanel (segmentation visibility/opacity)
│   ├── FlowToolPanel (4D Flow controls)
│   ├── ReportPanel (report generation)
│   └── WorkflowPanel (S/P mode, phase slider)
│
├── Dialogs
│   ├── SettingsDialog (log level, UI preferences)
│   ├── PacsConfigDialog (PACS server configuration)
│   ├── QuantificationWindow (flow/cardiac metrics)
│   ├── VideoExportDialog (video export parameters)
│   └── MaskWizard (multi-step segmentation wizard)
│
└── Widgets
    ├── PhaseSliderWidget (cardiac phase / temporal frame)
    ├── SPModeToggle (Single/Phase mode switching)
    ├── FlowGraphWidget (time-series chart, QPainter)
    └── WorkflowTabBar (tab-based workflow navigation)
```

### Key UI Patterns

- **Pimpl**: All major UI classes use pointer-to-implementation for ABI stability
- **Dock Widgets**: Panels are Qt dock widgets, allowing flexible layout
- **Signal/Slot**: Qt signal/slot mechanism for inter-component communication
- **MVC separation**: Controllers (e.g., `MaskWizardController`, `Display3DController`) separate logic from view

---

## Data Flow

### DICOM File to Rendered Image

```
1. FILE LOADING
   DICOM files --> DicomLoader.scanDirectory()
   --> SliceInfo[] (metadata per slice)

2. SERIES ASSEMBLY
   SliceInfo[] --> SeriesBuilder.buildVolume()
   --> itk::Image<short, 3> (sorted 3D volume)

3. ENHANCED DICOM (if multi-frame)
   Multi-frame file --> EnhancedDicomParser
   --> FrameExtractor --> per-frame volumes

4. PREPROCESSING (optional)
   itk::Image --> GaussianSmoother / N4BiasCorrector
   --> filtered itk::Image

5. FORMAT CONVERSION
   itk::Image --> ImageConverter --> vtkImageData

6. VOLUME RENDERING
   vtkImageData --> VolumeRenderer
   --> TransferFunction --> VTK render pipeline

7. MPR VIEWS
   vtkImageData --> MPRRenderer (3 planes)
   --> MPRCoordinateTransformer (crosshair sync)
```

### 4D Flow Pipeline

```
DICOM files --> FlowDicomParser --> VendorParser (Siemens/Philips/GE)
--> PhaseCorrector --> VelocityFieldAssembler
--> FlowVisualizer (streamlines) + FlowQuantifier (metrics)
--> VesselAnalyzer (WSS, OSI, TKE)
```

### Cardiac Pipeline

```
Enhanced CT --> CardiacPhaseDetector --> phase volumes
--> CalciumScorer (Agatston score)
--> CoronaryCenterlineExtractor --> CurvedPlanarReformatter (CPR)
```

### Export Pipeline

```
Volume/Mesh/Measurements --> DataExporter
--> NRRD, STL, OBJ, PLY, PDF, HTML, JSON, CSV,
    DICOM SR, Ensight, MATLAB .mat, OGG video
```

---

## Design Principles

### 1. Pimpl (Pointer to Implementation)

All major classes use the Pimpl idiom to hide implementation details, reduce compile times, and maintain ABI stability.

```cpp
class VolumeRenderer {
public:
    VolumeRenderer();
    ~VolumeRenderer();
    VolumeRenderer(VolumeRenderer&&) noexcept;
    VolumeRenderer& operator=(VolumeRenderer&&) noexcept;
    // public API ...
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
```

### 2. std::expected Error Handling (C++23)

Service methods return `std::expected<T, std::string>` for explicit, exception-free error propagation.

### 3. Strategy Pattern

Vendor-specific logic (e.g., flow DICOM parsing) uses the Strategy pattern with a common interface (`IVendorFlowParser`) and concrete implementations per vendor.

### 4. Command Pattern

Segmentation undo/redo uses the Command pattern. `SnapshotCommand` stores RLE-compressed label maps for memory-efficient undo history.

### 5. Service Layer Isolation

Each service is a separate CMake static library with its own source and header directories. Services communicate through well-defined interfaces, not internal implementation details.

---

## Dependencies

### External Libraries

| Library | Version | Purpose |
|---------|---------|---------|
| Qt6 | 6.5+ | UI framework (Widgets, OpenGL, Concurrent) |
| VTK | 9.3+ | 3D visualization and volume rendering |
| ITK | 5.4+ | Medical image processing and DICOM I/O |
| GDCM | - | Low-level DICOM tag parsing (Enhanced DICOM) |
| FFTW3 | - | FFT for N4 bias field correction |
| spdlog | - | High-performance logging |
| fmt | - | String formatting |
| nlohmann_json | - | JSON serialization |
| zlib | - | Compression (project files) |
| Google Test | - | Testing framework |

### kcenon Ecosystem

| Library | Purpose |
|---------|---------|
| pacs_system | DICOM network operations (SCU/SCP) |
| thread_system | Thread pool and job scheduling |
| logger_system | Structured logging infrastructure |
| common_system | Shared interfaces (IExecutor, Result<T>) |

---

## Performance Targets

| Metric | Target | Condition |
|--------|--------|-----------|
| CT Series Loading | <= 3 sec | 512 x 512 x 300 volume |
| Volume Rendering | >= 30 FPS | Interactive rotation/zoom |
| MPR Slice Switch | <= 100 ms | Mouse wheel scroll |
| Memory Usage | <= 2 GB | Single 1 GB volume loaded |
| Application Startup | <= 5 sec | Cold start |

---

## Test Infrastructure

**3071+ tests** across unit, integration, and benchmark categories.

| Category | Count | Examples |
|----------|-------|---------|
| Unit | 80+ executables | Each service module has dedicated tests |
| Integration | 4 executables | Flow pipeline, cardiac pipeline, series loading |
| Benchmark | 2 executables | Performance and rendering benchmarks |

All tests use Google Test with `gtest_discover_tests` for CTest integration.

---

**Date**: 2026-02-23
**Version**: 0.7.0-pre

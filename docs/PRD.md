# DICOM Viewer - Product Requirements Document (PRD)

> **Version**: 0.2.0
> **Created**: 2025-12-31
> **Last Updated**: 2025-12-31
> **Status**: Draft (Pre-release)
> **Author**: Development Team

---

## 1. Executive Summary

### 1.1 Product Vision

**DICOM Viewer** is a high-performance medical image viewer designed for medical imaging professionals. It prioritizes 3D volume rendering and MPR (Multi-Planar Reconstruction) views for CT and MRI images, while providing basic 2D viewing capabilities for DR/CR images.

### 1.2 Goals

| Goal | Description | Priority |
|------|-------------|----------|
| **CT/MRI 3D Visualization** | Volume rendering, surface rendering, MPR view | P0 (Critical) |
| **Region Segmentation** | Automatic/semi-automatic/manual segmentation tools, multi-label support, morphological post-processing | P1 (High) |
| **Region Measurement & Analysis** | ROI-based area/volume measurement, HU statistics, quantitative analysis, report generation | P1 (High) |
| **DR/CR 2D Viewing** | Basic 2D image viewing and Window/Level adjustment | P2 (Medium) |
| **PACS Integration** | PACS integration via C-FIND, C-MOVE, C-STORE | P1 (High) |

### 1.3 Success Criteria

- CT/MRI series loading time: **under 3 seconds** for 512x512x300 volume
- Volume rendering frame rate: **30 FPS or higher**
- Memory usage: **under 2GB** for 1GB volume data
- Supported compression formats: JPEG, JPEG 2000, JPEG-LS, RLE

---

## 2. Target Users

### 2.1 Primary Personas

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          User Personas                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ┌─────────────────────┐   ┌─────────────────────┐                    │
│   │     Radiologist     │   │  Researcher/        │                    │
│   │      (Primary)       │   │  Developer          │                    │
│   │                      │   │     (Secondary)      │                    │
│   │  • CT/MRI reading    │   │  • Algorithm        │                    │
│   │  • 3D reconstruction │   │    validation       │                    │
│   │    analysis          │   │  • Image processing │                    │
│   │  • Measurement/      │   │    research         │                    │
│   │    quantitative      │   │  • Segmentation     │                    │
│   │    analysis          │   │    algorithms       │                    │
│   │                      │   │                      │                    │
│   │  Needs:              │   │  Needs:              │                    │
│   │  - Fast loading      │   │  - API accessibility │                    │
│   │  - Intuitive UI      │   │  - Extensibility     │                    │
│   │  - Accurate          │   │  - Data export       │                    │
│   │    measurements      │   │                      │                    │
│   └─────────────────────┘   └─────────────────────┘                    │
│                                                                         │
│   ┌─────────────────────┐                                              │
│   │  Radiologic         │                                              │
│   │  Technologist       │                                              │
│   │     (Tertiary)       │                                              │
│   │                      │                                              │
│   │  • DR/CR image       │                                              │
│   │    verification      │                                              │
│   │  • Image quality     │                                              │
│   │    check             │                                              │
│   │  • PACS transfer     │                                              │
│   │                      │                                              │
│   │  Needs:              │                                              │
│   │  - Simple operation  │                                              │
│   │  - Quick review      │                                              │
│   │  - PACS integration  │                                              │
│   └─────────────────────┘                                              │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Use Cases

| ID | User | Scenario | Priority |
|----|------|----------|----------|
| UC-01 | Radiologist | Volume rendering of CT chest images to examine lung structure | P0 |
| UC-02 | Radiologist | Cross-sectional analysis of MRI brain images using MPR view | P0 |
| UC-03 | Radiologist | Tumor size measurement on CT images (distance, area, volume) | P1 |
| UC-04 | Researcher | 3D surface extraction after specific tissue segmentation | P1 |
| UC-05 | Technologist | Window/Level adjustment review of DR chest images | P2 |
| UC-06 | Technologist | Search and download images from PACS | P1 |
| UC-07 | Radiologist | Liver segmentation on CT images followed by volume measurement | P1 |
| UC-08 | Radiologist | HU statistics analysis (Mean, StdDev) after tumor ROI definition | P1 |
| UC-09 | Researcher | Multi-organ segmentation (liver, spleen, kidney) for comparative analysis | P1 |
| UC-10 | Researcher | Save segmentation results as NRRD and generate analysis report | P2 |

---

## 3. Functional Requirements

### 3.1 Priority Definitions

| Priority | Description | Release Target |
|----------|-------------|----------------|
| **P0 (Critical)** | MVP essential features | v0.3.0 |
| **P1 (High)** | Core value delivery | v0.4.0 |
| **P2 (Medium)** | Usability improvements | v0.5.0 |
| **P3 (Low)** | Future extensions | v1.0.0+ |

### 3.2 CT/MRI Viewing (P0 - Critical)

#### FR-001: DICOM Series Loading

```
┌─────────────────────────────────────────────────────────────────┐
│                    DICOM Loading Pipeline                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   ┌─────────────┐    ┌─────────────┐    ┌─────────────────────┐│
│   │ pacs_system │ →  │    ITK      │ →  │        VTK          ││
│   │             │    │             │    │                     ││
│   │ • DICOM I/O │    │ • 3D Volume │    │ • vtkImageData      ││
│   │ • Metadata  │    │   Assembly  │    │ • Direction Matrix  ││
│   │ • Decompress│    │ • HU Convert│    │                     ││
│   └─────────────┘    └─────────────┘    └─────────────────────┘│
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

| Requirement | Description |
|-------------|-------------|
| FR-001.1 | Load DICOM series from local directory |
| FR-001.2 | Automatic sorting based on Slice Location / Instance Number |
| FR-001.3 | Auto-detection and decoding of compression formats (JPEG, JPEG2000, RLE) |
| FR-001.4 | Metadata extraction (Patient, Study, Series, Image info) |
| FR-001.5 | HU value conversion using Rescale Slope/Intercept |

#### FR-002: Volume Rendering

| Requirement | Description |
|-------------|-------------|
| FR-002.1 | GPU-accelerated Ray Casting volume rendering |
| FR-002.2 | Color Transfer Function editing |
| FR-002.3 | Opacity Transfer Function editing |
| FR-002.4 | Pre-defined presets (CT Bone, CT Soft Tissue, CT Lung, CT Angio, MRI) |
| FR-002.5 | Real-time lighting and shading adjustment |
| FR-002.6 | Volume clipping (Box Widget) |

**CT Transfer Function Presets:**

| Preset | Window Width | Window Center | Purpose |
|--------|--------------|---------------|---------|
| CT Bone | 2000 | 400 | Bone structure |
| CT Soft Tissue | 400 | 40 | Soft tissue |
| CT Lung | 1500 | -600 | Lung |
| CT Angio | 400 | 200 | Vessels |
| CT Abdomen | 400 | 50 | Abdomen |

#### FR-003: MPR (Multi-Planar Reconstruction)

```
┌─────────────────────────────────────────────────────────────────┐
│                         MPR Layout                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   ┌───────────────────┐   ┌───────────────────┐                │
│   │      Axial        │   │     Coronal       │                │
│   │       (XY)        │   │       (XZ)        │                │
│   │                   │   │                   │                │
│   │    ┌─────────┐    │   │    ┌─────────┐    │                │
│   │    │  Top    │    │   │    │  Front  │    │                │
│   │    │  View   │    │   │    │  View   │    │                │
│   │    └─────────┘    │   │    └─────────┘    │                │
│   └───────────────────┘   └───────────────────┘                │
│                                                                 │
│   ┌───────────────────┐   ┌───────────────────┐                │
│   │     Sagittal      │   │    3D Volume      │                │
│   │       (YZ)        │   │   or Oblique      │                │
│   │                   │   │                   │                │
│   │    ┌─────────┐    │   │    ┌─────────┐    │                │
│   │    │  Side   │    │   │    │  3D/Obl │    │                │
│   │    │  View   │    │   │    │  View   │    │                │
│   │    └─────────┘    │   │    └─────────┘    │                │
│   └───────────────────┘   └───────────────────┘                │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

| Requirement | Description |
|-------------|-------------|
| FR-003.1 | Simultaneous display of Axial, Coronal, Sagittal (2x2 layout) |
| FR-003.2 | Slice scrolling (mouse wheel, slider) |
| FR-003.3 | Crosshair synchronization (click in one view updates others) |
| FR-003.4 | Window/Level adjustment (mouse drag) |
| FR-003.5 | Oblique reslicing (arbitrary angle cross-section) |
| FR-003.6 | Thick Slab (MIP, MinIP, AverageIP) |

#### FR-004: Surface Rendering

| Requirement | Description |
|-------------|-------------|
| FR-004.1 | Marching Cubes isosurface extraction |
| FR-004.2 | Threshold-based tissue surface generation |
| FR-004.3 | Surface smoothing and decimation |
| FR-004.4 | Multi-surface simultaneous rendering (color-coded) |
| FR-004.5 | STL/PLY export |

### 3.3 Image Processing (P1 - High)

#### FR-005: Preprocessing

| Requirement | Description |
|-------------|-------------|
| FR-005.1 | Gaussian Smoothing (noise reduction) |
| FR-005.2 | Anisotropic Diffusion (edge-preserving smoothing) |
| FR-005.3 | Histogram Equalization (contrast enhancement) |
| FR-005.4 | N4 Bias Field Correction (MRI inhomogeneity correction) |
| FR-005.5 | Isotropic Resampling (isotropic voxel conversion) |

#### FR-006: Segmentation

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      Segmentation Workflow                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ┌───────────────────────────────────────────────────────────────────┐│
│   │                    1. ROI Selection                                ││
│   │   ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────────┐  ││
│   │   │  Manual  │  │  Semi-   │  │   Auto   │  │   AI-Assisted    │  ││
│   │   │   ROI    │  │   Auto   │  │ Threshold│  │   (Future)       │  ││
│   │   └──────────┘  └──────────┘  └──────────┘  └──────────────────┘  ││
│   └───────────────────────────────────────────────────────────────────┘│
│                              │                                          │
│                              ↓                                          │
│   ┌───────────────────────────────────────────────────────────────────┐│
│   │                    2. Segmentation Tools                           ││
│   │   ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────────┐  ││
│   │   │  Brush   │  │  Eraser  │  │  Fill    │  │   Smart Select   │  ││
│   │   │   Tool   │  │   Tool   │  │  Tool    │  │   (Grow from     │  ││
│   │   │          │  │          │  │          │  │    click point)  │  ││
│   │   └──────────┘  └──────────┘  └──────────┘  └──────────────────┘  ││
│   └───────────────────────────────────────────────────────────────────┘│
│                              │                                          │
│                              ↓                                          │
│   ┌───────────────────────────────────────────────────────────────────┐│
│   │                    3. Post-Processing                              ││
│   │   ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────────┐  ││
│   │   │  Smooth  │  │   Fill   │  │  Dilate/ │  │   Island         │  ││
│   │   │  Edges   │  │  Holes   │  │  Erode   │  │   Removal        │  ││
│   │   └──────────┘  └──────────┘  └──────────┘  └──────────────────┘  ││
│   └───────────────────────────────────────────────────────────────────┘│
│                              │                                          │
│                              ↓                                          │
│   ┌───────────────────────────────────────────────────────────────────┐│
│   │                    4. Analysis & Export                            ││
│   │   ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────────┐  ││
│   │   │  Volume  │  │ Surface  │  │  Stats   │  │   Export         │  ││
│   │   │  Calc    │  │ Extract  │  │  Report  │  │   (STL/NRRD)     │  ││
│   │   └──────────┘  └──────────┘  └──────────┘  └──────────────────┘  ││
│   └───────────────────────────────────────────────────────────────────┘│
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**FR-006.A: Automatic/Semi-automatic Segmentation Algorithms**

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-006.1 | Manual threshold segmentation (Min/Max HU range specification) | P1 |
| FR-006.2 | Otsu automatic threshold (single/multi-class) | P1 |
| FR-006.3 | Region Growing (seed point based, click to set seed) | P1 |
| FR-006.4 | Confidence Connected (automatic threshold Region Growing) | P1 |
| FR-006.5 | Level Set segmentation (Geodesic Active Contour) | P2 |
| FR-006.6 | Watershed segmentation (boundary-based) | P2 |

**FR-006.B: Manual Segmentation Tools**

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-006.7 | Brush tool (adjustable size, circular/rectangular) | P1 |
| FR-006.8 | Eraser tool (delete segmentation region) | P1 |
| FR-006.9 | Fill tool (fill closed region) | P1 |
| FR-006.10 | Freehand curve tool (Freehand drawing) | P1 |
| FR-006.11 | Polygon tool (Polygon ROI) | P1 |
| FR-006.12 | Smart scissors tool (boundary tracing) | P2 |

**FR-006.C: Segmentation Editing and Management**

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-006.13 | Multi-label support (simultaneous segmentation of multiple organs/tissues) | P1 |
| FR-006.14 | Per-label color assignment | P1 |
| FR-006.15 | Undo/Redo | P1 |
| FR-006.16 | Per-slice segmentation → 3D interpolation | P2 |
| FR-006.17 | Save segmentation results (NRRD, NIfTI format) | P1 |
| FR-006.18 | Load segmentation results | P1 |

**FR-006.D: Morphological Post-processing**

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-006.19 | Opening operation - remove small protrusions | P1 |
| FR-006.20 | Closing operation - fill small holes | P1 |
| FR-006.21 | Dilation | P1 |
| FR-006.22 | Erosion | P1 |
| FR-006.23 | Fill Holes | P1 |
| FR-006.24 | Island Removal (size threshold specification) | P1 |
| FR-006.25 | Keep Largest Connected Component | P1 |

**Segmentation Target Presets (CT):**

| Preset | HU Range | Purpose | Algorithm |
|--------|----------|---------|-----------|
| Bone | 200 ~ 3000 | Bone extraction | Threshold + Morphology |
| Soft Tissue | -100 ~ 200 | Soft tissue | Region Growing |
| Lung | -950 ~ -400 | Lung parenchyma | Threshold + Hole Fill |
| Liver | 40 ~ 80 | Liver | Region Growing |
| Vessel (Contrast) | 150 ~ 500 | Vessels (contrast agent) | Threshold |
| Fat | -150 ~ -50 | Fat | Threshold |

#### FR-007: Measurement

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      Measurement Tools                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐  │
│   │                    Linear Measurements                           │  │
│   │                                                                   │  │
│   │   ┌────────────┐   ┌────────────┐   ┌────────────────────────┐  │  │
│   │   │  Distance  │   │   Angle    │   │   Cobb Angle           │  │  │
│   │   │   (2pts)   │   │   (3pts)   │   │   (Spine analysis)     │  │  │
│   │   │    ↔       │   │     ∠      │   │      ⟨  ⟩             │  │  │
│   │   └────────────┘   └────────────┘   └────────────────────────┘  │  │
│   │                                                                   │  │
│   └─────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐  │
│   │                      Area Measurements                           │  │
│   │                                                                   │  │
│   │   ┌────────────┐   ┌────────────┐   ┌────────────────────────┐  │  │
│   │   │  Ellipse   │   │  Polygon   │   │   Freehand ROI         │  │  │
│   │   │    ROI     │   │    ROI     │   │                        │  │  │
│   │   │     ⬭     │   │     ⬡      │   │      ～～～            │  │  │
│   │   └────────────┘   └────────────┘   └────────────────────────┘  │  │
│   │                                                                   │  │
│   └─────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐  │
│   │                    Volume Measurements                           │  │
│   │                                                                   │  │
│   │   ┌────────────────────────────────────────────────────────────┐│  │
│   │   │   Segmented Volume   │   3D ROI Box   │   Spherical ROI   ││  │
│   │   │        ▣            │       ⬜        │         ⬤         ││  │
│   │   └────────────────────────────────────────────────────────────┘│  │
│   │                                                                   │  │
│   └─────────────────────────────────────────────────────────────────┘  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**FR-007.A: Linear Measurements**

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-007.1 | Distance measurement (between 2 points, mm unit) | P1 |
| FR-007.2 | Multiple distance measurements (continuous measurement) | P1 |
| FR-007.3 | Angle measurement (3 points, degree unit) | P1 |
| FR-007.4 | Cobb angle measurement (4 points for spine analysis) | P2 |
| FR-007.5 | Measurement annotation display (value overlay on screen) | P1 |

**FR-007.B: Area and ROI Measurements**

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-007.6 | Circular/Elliptical ROI area measurement (mm²) | P1 |
| FR-007.7 | Rectangular ROI area measurement | P1 |
| FR-007.8 | Polygon ROI area measurement | P1 |
| FR-007.9 | Freehand ROI area measurement | P1 |
| FR-007.10 | ROI perimeter measurement (mm) | P1 |

**FR-007.C: Volume Measurements**

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-007.11 | Segmented region volume calculation (mm³, cm³, mL) | P1 |
| FR-007.12 | 3D Bounding Box volume | P2 |
| FR-007.13 | Spherical ROI volume | P2 |
| FR-007.14 | Multi-label volume comparison | P1 |

**FR-007.D: ROI Statistics Analysis**

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-007.15 | Mean value (Mean HU/Signal Intensity) | P1 |
| FR-007.16 | Standard Deviation | P1 |
| FR-007.17 | Min/Max values | P1 |
| FR-007.18 | Median | P1 |
| FR-007.19 | Histogram display | P1 |
| FR-007.20 | Pixel/Voxel count | P1 |

**FR-007.E: Advanced Quantitative Analysis**

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-007.21 | Segmented region surface area calculation (mm²) | P2 |
| FR-007.22 | Sphericity calculation | P2 |
| FR-007.23 | Elongation calculation | P2 |
| FR-007.24 | Center of mass coordinates | P2 |
| FR-007.25 | Principal Axes direction | P3 |

#### FR-012: ROI Management - P1

| Requirement | Description |
|-------------|-------------|
| FR-012.1 | ROI list panel (display all created ROIs) |
| FR-012.2 | ROI naming and editing |
| FR-012.3 | ROI show/hide toggle |
| FR-012.4 | ROI color change |
| FR-012.5 | ROI copy/paste (to other slices) |
| FR-012.6 | ROI deletion |
| FR-012.7 | ROI save (DICOM SR, JSON, CSV) |
| FR-012.8 | ROI loading |

#### FR-013: Analysis Report Generation - P2

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      Analysis Report Template                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Patient: [Name]              Study Date: [Date]                       │
│   Patient ID: [ID]             Modality: [CT/MRI]                       │
│   ─────────────────────────────────────────────────────────────────    │
│                                                                         │
│   Segmentation Results:                                                 │
│   ┌─────────────────────────────────────────────────────────────────┐  │
│   │  Label        │  Volume (cm³)  │  Mean HU  │  Surface (cm²)    │  │
│   │──────────────────────────────────────────────────────────────── │  │
│   │  Liver        │     1523.5     │    55.2   │     845.3         │  │
│   │  Tumor        │      12.8      │    42.1   │      28.6         │  │
│   │  Bone         │     2341.2     │   456.8   │    1204.5         │  │
│   └─────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│   ROI Measurements:                                                     │
│   ┌─────────────────────────────────────────────────────────────────┐  │
│   │  ROI Name     │  Type    │  Value    │  Location              │  │
│   │──────────────────────────────────────────────────────────────── │  │
│   │  Distance 1   │  Length  │  45.2 mm  │  Slice 120             │  │
│   │  Lesion ROI   │  Area    │  125 mm²  │  Slice 145             │  │
│   │  Angle 1      │  Angle   │  32.5°    │  Slice 120             │  │
│   └─────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│   [Screenshot thumbnails]                                               │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

| Requirement | Description |
|-------------|-------------|
| FR-013.1 | Segmentation results summary (volume, mean value, surface area) |
| FR-013.2 | Measurement results list |
| FR-013.3 | Include screenshots |
| FR-013.4 | PDF export |
| FR-013.5 | CSV/Excel export (data only) |
| FR-013.6 | DICOM SR (Structured Report) save |

### 3.4 DR/CR Viewing (P2 - Medium)

#### FR-008: 2D Image Viewing

| Requirement | Description |
|-------------|-------------|
| FR-008.1 | Single frame DICOM loading |
| FR-008.2 | Window/Level adjustment |
| FR-008.3 | Zoom and pan |
| FR-008.4 | Rotation (90-degree increments, free rotation) |
| FR-008.5 | Horizontal flip, vertical flip |
| FR-008.6 | Grid view (multiple image comparison) |

#### FR-009: 2D Image Presets

| Preset | Window Width | Window Center | Purpose |
|--------|--------------|---------------|---------|
| Chest | 1500 | -600 | Chest X-ray |
| Abdomen | 400 | 40 | Abdomen X-ray |
| Bone | 2000 | 300 | Skeletal X-ray |
| Mammography | 2048 | 1024 | Mammography |

### 3.5 PACS Integration (P1 - High)

#### FR-010: Network Services

| Requirement | Description |
|-------------|-------------|
| FR-010.1 | C-ECHO (connection test) |
| FR-010.2 | C-FIND (Patient/Study/Series/Image level search) |
| FR-010.3 | C-MOVE (image retrieval) |
| FR-010.4 | C-STORE SCP (image reception) |
| FR-010.5 | PACS server configuration management (AE Title, Host, Port) |

### 3.6 UI/UX (P1 - High)

#### FR-011: User Interface

```
┌─────────────────────────────────────────────────────────────────────────┐
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                        Menu Bar                                  │   │
│  │  File  Edit  View  Tools  Image  Window  Help                   │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                        Tool Bar                                  │   │
│  │  [Open] [Save] [PACS] │ [Scroll] [Zoom] [Pan] [W/L] [Measure]   │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│  ┌───────────┬─────────────────────────────────────┬───────────────┐   │
│  │           │                                     │               │   │
│  │  Patient  │                                     │   Tools /     │   │
│  │   List    │         Main Viewport               │   Properties  │   │
│  │           │                                     │               │   │
│  │  ───────  │    ┌─────────────────────────────┐  │   ─────────   │   │
│  │  Studies  │    │                             │  │   Window/     │   │
│  │  Series   │    │     2D / 3D View Area       │  │   Level       │   │
│  │  Images   │    │                             │  │   ─────────   │   │
│  │           │    │                             │  │   Transfer    │   │
│  │           │    │                             │  │   Function    │   │
│  │           │    └─────────────────────────────┘  │   ─────────   │   │
│  │           │                                     │   Measure     │   │
│  └───────────┴─────────────────────────────────────┴───────────────┘   │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                      Status Bar                                  │   │
│  │  Patient: xxx  |  Series: xxx  |  Slice: 150/300  |  HU: 40     │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

| Requirement | Description |
|-------------|-------------|
| FR-011.1 | Qt6-based native UI |
| FR-011.2 | Dark theme default support |
| FR-011.3 | Dockable panel structure |
| FR-011.4 | Keyboard shortcuts support |
| FR-011.5 | Multi-monitor support |
| FR-011.6 | Layout save/restore |

---

## 4. Non-Functional Requirements

### 4.1 Performance

| ID | Requirement | Target |
|----|-------------|--------|
| NFR-001 | CT series loading (512x512x300) | ≤ 3 seconds |
| NFR-002 | Volume rendering FPS | ≥ 30 FPS |
| NFR-003 | MPR slice transition response | ≤ 100ms |
| NFR-004 | Memory usage (1GB volume) | ≤ 2GB |
| NFR-005 | Startup time (Cold Start) | ≤ 5 seconds |

### 4.2 Compatibility

| ID | Requirement | Details |
|----|-------------|---------|
| NFR-006 | Operating System | macOS 12+, Windows 10+, Ubuntu 20.04+ |
| NFR-007 | Graphics Card | GPU supporting OpenGL 4.1+ |
| NFR-008 | DICOM Standard | DICOM PS3.x compliant |
| NFR-009 | Transfer Syntax | JPEG, JPEG2000, JPEG-LS, RLE |

### 4.3 Usability

| ID | Requirement |
|----|-------------|
| NFR-010 | Main functions accessible within 3 clicks |
| NFR-011 | New users can learn basic functions within 30 minutes |
| NFR-012 | Clear error messages when errors occur |

### 4.4 Security

| ID | Requirement |
|----|-------------|
| NFR-013 | Patient information anonymization feature in DICOM files |
| NFR-014 | TLS encryption support for PACS communication |
| NFR-015 | Local configuration file encryption |

---

## 5. Technology Stack

### 5.1 Core Technologies

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         Technology Stack                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐  │
│   │                      Presentation Layer                          │  │
│   │                                                                   │  │
│   │    Qt6 Widgets + QVTKOpenGLNativeWidget                          │  │
│   │                                                                   │  │
│   └─────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│   ┌─────────────────────────────────────────────────────────────────┐  │
│   │                     Visualization Layer                          │  │
│   │                                                                   │  │
│   │    VTK 9.x                                                        │  │
│   │    • vtkGPUVolumeRayCastMapper (Volume rendering)                │  │
│   │    • vtkImageReslice (MPR)                                        │  │
│   │    • vtkMarchingCubes (Surface extraction)                       │  │
│   │    • vtkImageViewer2 (2D view)                                    │  │
│   │    • Interactive widgets                                          │  │
│   │                                                                   │  │
│   └─────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│   ┌─────────────────────────────────────────────────────────────────┐  │
│   │                      Processing Layer                            │  │
│   │                                                                   │  │
│   │    ITK 5.x                                                        │  │
│   │    • Image filtering (Gaussian, Anisotropic Diffusion)           │  │
│   │    • Image segmentation (Region Growing, Level Sets)             │  │
│   │    • Image registration                                           │  │
│   │    • N4 Bias Field Correction                                    │  │
│   │                                                                   │  │
│   └─────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│   ┌─────────────────────────────────────────────────────────────────┐  │
│   │                        Data Layer                                │  │
│   │                                                                   │  │
│   │    pacs_system (kcenon ecosystem)                                │  │
│   │    • DICOM file parsing                                          │  │
│   │    • Metadata extraction                                          │  │
│   │    • Transfer Syntax decoding                                     │  │
│   │    • DICOM network services (C-FIND, C-MOVE, C-STORE)            │  │
│   │                                                                   │  │
│   └─────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐  │
│   │                     Foundation Layer                             │  │
│   │                                                                   │  │
│   │    C++20 / CMake 3.20+                                           │  │
│   │    kcenon ecosystem (common_system, container_system,            │  │
│   │                       thread_system, network_system)             │  │
│   │                                                                   │  │
│   └─────────────────────────────────────────────────────────────────┘  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 5.2 Version Requirements

| Component | Version | Purpose |
|-----------|---------|---------|
| **C++** | C++20 | Language standard |
| **CMake** | 3.20+ | Build system |
| **Qt** | 6.5+ | GUI framework |
| **VTK** | 9.3+ | Visualization |
| **ITK** | 5.4+ | Image processing |
| **pacs_system** | Latest | DICOM processing |

### 5.3 Build Dependencies

```cmake
# vcpkg.json
{
    "dependencies": [
        { "name": "itk", "features": ["vtk"] },
        { "name": "vtk", "features": ["qt"] },
        { "name": "qt6" },
        { "name": "pacs-system" }
    ]
}
```

---

## 6. Architecture Overview

### 6.1 High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        DICOM Viewer Architecture                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐  │
│   │                          UI Layer                                │  │
│   │                                                                   │  │
│   │  MainWindow ─┬─ PatientBrowser (Patient/Series navigation)       │  │
│   │              ├─ ViewportManager (Viewport management)            │  │
│   │              ├─ ToolsPanel (Tools panel)                         │  │
│   │              └─ StatusBar (Status display)                       │  │
│   │                                                                   │  │
│   └─────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│                              ↓                                          │
│   ┌─────────────────────────────────────────────────────────────────┐  │
│   │                      Controller Layer                            │  │
│   │                                                                   │  │
│   │  ViewerController ─┬─ LoadingController (Loading control)        │  │
│   │                    ├─ RenderingController (Rendering control)    │  │
│   │                    ├─ ToolController (Tool control)              │  │
│   │                    └─ NetworkController (PACS control)           │  │
│   │                                                                   │  │
│   └─────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│                              ↓                                          │
│   ┌─────────────────────────────────────────────────────────────────┐  │
│   │                       Service Layer                              │  │
│   │                                                                   │  │
│   │  ┌───────────────┐  ┌───────────────┐  ┌───────────────────────┐│  │
│   │  │ ImageService  │  │ RenderService │  │    NetworkService     ││  │
│   │  │               │  │               │  │                       ││  │
│   │  │ • Loading     │  │ • Volume      │  │ • C-FIND              ││  │
│   │  │ • Processing  │  │ • Surface     │  │ • C-MOVE              ││  │
│   │  │ • Segmentation│  │ • MPR         │  │ • C-STORE             ││  │
│   │  │ • Measurement │  │ • 2D View     │  │                       ││  │
│   │  └───────────────┘  └───────────────┘  └───────────────────────┘│  │
│   │                                                                   │  │
│   └─────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│                              ↓                                          │
│   ┌─────────────────────────────────────────────────────────────────┐  │
│   │                        Data Layer                                │  │
│   │                                                                   │  │
│   │  ┌───────────────┐  ┌───────────────┐  ┌───────────────────────┐│  │
│   │  │ ImageData     │  │ DicomData     │  │     MetaData          ││  │
│   │  │               │  │               │  │                       ││  │
│   │  │ • ITK Image   │  │ • pacs_system │  │ • Patient Info        ││  │
│   │  │ • VTK Image   │  │   Dataset     │  │ • Study Info          ││  │
│   │  │               │  │               │  │ • Series Info         ││  │
│   │  └───────────────┘  └───────────────┘  └───────────────────────┘│  │
│   │                                                                   │  │
│   └─────────────────────────────────────────────────────────────────┘  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 6.2 Module Dependencies

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        Module Dependencies                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   dicom_viewer_ui                                                       │
│        │                                                                │
│        ↓                                                                │
│   dicom_viewer_controller ←─────────────────────────────────┐          │
│        │                                                    │          │
│        ↓                                                    │          │
│   ┌────┴────┬────────────────┬────────────────┐             │          │
│   ↓         ↓                ↓                ↓             │          │
│   image_    render_          network_         measurement_  │          │
│   service   service          service          service       │          │
│   │         │                │                │             │          │
│   └────┬────┴────────────────┴────────────────┘             │          │
│        ↓                                                    │          │
│   dicom_viewer_core (common data structures, utilities) ────┘          │
│        │                                                                │
│        ↓                                                                │
│   ┌────┴────┬────────────────┬────────────────┐                        │
│   ↓         ↓                ↓                ↓                        │
│   pacs_     ITK              VTK              Qt6                       │
│   system                                                                │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 7. Milestone Plan

### 7.1 Release Schedule

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          Release Timeline                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Phase 1: Foundation        Phase 2: Core        Phase 3: Enhancement  │
│   ─────────────────          ──────────────       ──────────────────    │
│                                                                         │
│   v0.1 ──┬── v0.2 ──┬── v0.3 ──┬── v0.4 ──┬── v0.5 ──┬── v1.0         │
│   (Pre)  │  (Pre)   │  (MVP)   │  (Core)  │  (Enh)   │  (Release)      │
│   ┌──────┘   ┌──────┘   ┌──────┘   ┌──────┘   ┌──────┘                 │
│   │          │          │          │          │                         │
│   Week 1-2   Week 3-4   Week 5-8   Week 9-12  Week 13-16               │
│                                                                         │
│   • Project  • DICOM    • MPR View • PACS     • DR/CR                  │
│     setup      loading  • Volume     integr.  • Advanced               │
│   • Basic UI • ITK/VTK    rendering• Measure    segment.               │
│              integr.   • Surface   • Basic   • Quant.                  │
│                          rendering   segment.   analysis               │
│                        • Presets              • Reports                │
│                                                                         │
│   ※ v0.x.x: Pre-release (in development) / v1.0.0+: Official release  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 7.2 Milestone Details

#### Milestone 1: Foundation (v0.1-v0.2) - Week 1-4

| Item | Description | Completion Criteria |
|------|-------------|---------------------|
| M1.1 | Project structure setup | CMake build success |
| M1.2 | Qt6 basic UI | MainWindow, basic layout |
| M1.3 | pacs_system integration | Single DICOM file load |
| M1.4 | ITK integration | pacs → ITK 3D volume conversion |
| M1.5 | VTK integration | ITK → VTK conversion, basic rendering |

#### Milestone 2: MVP - CT/MRI Viewer (v0.3.0) - Week 5-8

| Item | Description | Completion Criteria |
|------|-------------|---------------------|
| M2.1 | DICOM series loading | 512x512x300 volume load in under 3 seconds |
| M2.2 | Volume rendering | GPU Ray Casting, 30 FPS |
| M2.3 | MPR view | Axial/Coronal/Sagittal synchronization |
| M2.4 | Surface rendering | Marching Cubes, smoothing |
| M2.5 | Presets | CT Bone, CT Soft Tissue, CT Lung |
| M2.6 | Window/Level | Mouse drag adjustment |

#### Milestone 3: Core Features (v0.4.0) - Week 9-12

| Item | Description | Completion Criteria |
|------|-------------|---------------------|
| M3.1 | PACS C-FIND | Patient/Study/Series search |
| M3.2 | PACS C-MOVE | Image retrieval |
| M3.3 | Linear measurement tools | Distance (2pts), Angle (3pts), value overlay |
| M3.4 | ROI measurement tools | Circular/Polygon/Freehand ROI, area measurement |
| M3.5 | ROI statistics analysis | Mean, StdDev, Min/Max, Histogram |
| M3.6 | Basic segmentation tools | Threshold segmentation, Otsu auto-threshold |
| M3.7 | Region Growing | Seed-based region growing, Confidence Connected |
| M3.8 | Manual segmentation tools | Brush, Eraser, Fill tools |
| M3.9 | Morphological post-processing | Opening, Closing, Fill Holes, Island Removal |
| M3.10 | Preprocessing | Gaussian, Anisotropic Diffusion, N4 |

#### Milestone 4: Enhancement (v0.5.0) - Week 13-16

| Item | Description | Completion Criteria |
|------|-------------|---------------------|
| M4.1 | DR/CR viewing | 2D image load and manipulation |
| M4.2 | Advanced segmentation | Level Set, Watershed |
| M4.3 | Multi-label segmentation | Multiple organ simultaneous segmentation, label management |
| M4.4 | Segmentation save/load | NRRD, NIfTI format support |
| M4.5 | ROI management | ROI list, name/color, save/load |
| M4.6 | Volume measurement | Segmented region volume, surface area calculation |
| M4.7 | Advanced quantitative analysis | Sphericity, Elongation, Center of mass |
| M4.8 | Analysis report | PDF/CSV export, DICOM SR |
| M4.9 | Image registration | Rigid Registration |
| M4.10 | STL export | Mesh for 3D printing |
| M4.11 | User settings | Layout, preset save |

---

## 8. Risks & Mitigations

### 8.1 Technical Risks

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| GPU compatibility issues | High | Medium | CPU fallback with vtkSmartVolumeMapper |
| Memory shortage for large data | High | Medium | Streaming loading, LOD implementation |
| ITK-VTK version compatibility | Medium | Low | Version pinning with vcpkg |
| Compression format decoding failure | Medium | Low | Pre-validate pacs_system codecs |

### 8.2 Schedule Risks

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| VTK/ITK learning curve | Medium | High | Utilize example code based on reference documentation |
| PACS integration complexity | Medium | Medium | Build unit test environment |
| UI/UX requirement changes | Low | Medium | Flexible structure with docking panels |

---

## 9. Appendix

### A. Supported DICOM Modalities

| Modality | Code | Priority | 3D Support | Features |
|----------|------|----------|------------|----------|
| CT | CT | P0 | Yes | Volume rendering, MPR, HU |
| MRI | MR | P0 | Yes | Volume rendering, MPR, multi-sequence |
| DR (Digital Radiography) | DX | P2 | No | 2D viewing |
| CR (Computed Radiography) | CR | P2 | No | 2D viewing |
| PET | PT | P3 | Yes | Fusion view (future) |
| US (Ultrasound) | US | P3 | Limited | 2D viewing (future) |

### B. DICOM Tag Reference

For key DICOM tags, see [04-dicom-pipeline.md](reference/04-dicom-pipeline.md#22-key-dicom-tags).

### C. Coordinate Systems

For coordinate system information, see [03-itk-vtk-integration.md](reference/03-itk-vtk-integration.md#3-coordinate-system-integration).

### D. Related Documentation

| Document | Location | Purpose |
|----------|----------|---------|
| ITK Overview | [01-itk-overview.md](reference/01-itk-overview.md) | ITK architecture and API |
| VTK Overview | [02-vtk-overview.md](reference/02-vtk-overview.md) | VTK architecture and API |
| ITK-VTK Integration | [03-itk-vtk-integration.md](reference/03-itk-vtk-integration.md) | Integration guide |
| DICOM Pipeline | [04-dicom-pipeline.md](reference/04-dicom-pipeline.md) | Processing pipeline |
| pacs_system Integration | [05-pacs-integration.md](reference/05-pacs-integration.md) | PACS integration |

---

## Version History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1.0 | 2025-12-31 | Development Team | Initial PRD |
| 0.2.0 | 2025-12-31 | Development Team | Detailed segmentation and region measurement features (FR-006, FR-007 expansion, FR-012, FR-013 addition) |

> **Note**: v0.x.x are Pre-release versions. Official releases start from v1.0.0.

---

*This document is subject to change based on stakeholder feedback and technical discoveries during development.*

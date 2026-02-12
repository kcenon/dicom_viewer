# DICOM Viewer - Product Requirements Document (PRD)

> **Version**: 0.5.0
> **Created**: 2025-12-31
> **Last Updated**: 2026-02-12
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
| **4D Flow MRI Analysis** | Velocity-encoded blood flow visualization and quantification | P1 (High) |
| **Enhanced DICOM IOD** | Multi-frame Enhanced CT/MR IOD parsing for modern scanner compatibility | P0 (Critical) |
| **Cardiac CT Analysis** | ECG-gated reconstruction, coronary CTA, calcium scoring | P1 (High) |
| **Cine MRI** | Multi-phase cardiac cine MRI temporal display and analysis | P2 (Medium) |
| **DR/CR 2D Viewing** | Basic 2D image viewing and Window/Level adjustment | P2 (Medium) |
| **PACS Integration** | PACS integration via C-FIND, C-MOVE, C-STORE | P1 (High) |

### 1.3 Success Criteria

- CT/MRI series loading time: **under 3 seconds** for 512x512x300 volume
- Volume rendering frame rate: **30 FPS or higher**
- Memory usage: **under 2GB** for 1GB volume data
- Supported compression formats: JPEG, JPEG 2000, JPEG-LS, RLE
- 4D Flow MRI: **under 15 seconds** for standard dataset (≤2 GB), cine playback at **15 FPS or higher**
- Enhanced DICOM multi-frame: **under 5 seconds** for 1000-frame Enhanced CT/MR dataset
- Cardiac CT multi-phase: **under 5 seconds** for 10-phase ECG-gated dataset

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
| UC-11 | Radiologist | Visualize aortic blood flow from 4D Flow MRI with streamlines and measure flow rate at cross-section | P1 |
| UC-12 | Researcher | Analyze Wall Shear Stress distribution on vessel wall from 4D Flow MRI data | P2 |
| UC-13 | Radiologist | Review coronary CTA with centerline extraction and CPR for stenosis grading | P1 |
| UC-14 | Radiologist | Calculate Agatston calcium score from non-contrast cardiac CT | P1 |
| UC-15 | Radiologist | Evaluate cardiac wall motion from cine MRI with temporal playback | P2 |
| UC-16 | Radiologist | Load Enhanced DICOM multi-frame dataset from modern Siemens/Philips scanner | P0 |

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

### 3.7 4D Flow MRI Analysis (P1 - High)

#### FR-014: 4D Flow MRI

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    4D Flow MRI Pipeline                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ┌──────────────┐   ┌──────────────┐   ┌──────────────────────────┐  │
│   │ DICOM Parse  │ → │ Vector Field │ → │    Phase Correction      │  │
│   │              │   │  Assembly    │   │                          │  │
│   │ • Siemens    │   │ • Mag + Vx,  │   │ • Aliasing unwrap       │  │
│   │ • Philips    │   │   Vy, Vz     │   │ • Eddy current          │  │
│   │ • GE         │   │ • VENC scale │   │ • Maxwell terms         │  │
│   └──────────────┘   └──────────────┘   └──────────────────────────┘  │
│                                                │                       │
│                          ┌─────────────────────┘                       │
│                          ↓                                             │
│   ┌──────────────────────────────────────────────────────────────────┐│
│   │                     Visualization & Analysis                       ││
│   │                                                                    ││
│   │   ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────────┐ ││
│   │   │Streamline│   │ Pathline │   │  Vector  │   │  Temporal    │ ││
│   │   │          │   │          │   │  Glyph   │   │  Navigation  │ ││
│   │   └──────────┘   └──────────┘   └──────────┘   └──────────────┘ ││
│   │                                                                    ││
│   │   ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────────┐ ││
│   │   │Flow Rate │   │   WSS    │   │ Pressure │   │    Report    │ ││
│   │   │          │   │          │   │ Gradient │   │   Export     │ ││
│   │   └──────────┘   └──────────┘   └──────────┘   └──────────────┘ ││
│   └──────────────────────────────────────────────────────────────────┘│
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**FR-014.A: 4D Flow DICOM Parsing and Data Assembly**

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-014.1 | Load 4D Flow MRI DICOM series with vendor-specific parsing (Siemens, Philips, GE) | P1 |
| FR-014.2 | Identify and sort velocity-encoded components (Magnitude, Vx, Vy, Vz) per cardiac phase | P1 |
| FR-014.3 | Assemble 3D velocity vector fields from encoded components with VENC scaling | P1 |
| FR-014.4 | Apply phase correction (velocity aliasing unwrap, eddy current, Maxwell terms) | P1 |

**FR-014.B: Flow Visualization**

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-014.5 | Streamline rendering showing instantaneous flow trajectories | P1 |
| FR-014.6 | Pathline rendering showing particle motion across cardiac phases | P1 |
| FR-014.7 | Vector glyph display (arrow-shaped markers for velocity direction and magnitude) | P1 |
| FR-014.8 | Color mapping by velocity magnitude, direction, or component | P1 |

**FR-014.C: Temporal Navigation**

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-014.9 | Cardiac phase slider with playback controls (play/pause/stop) | P1 |
| FR-014.10 | Cine mode with configurable frame rate (1-30 fps) | P1 |
| FR-014.11 | ECG-gated timeline display | P2 |

**FR-014.D: Flow Quantification**

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-014.12 | Flow rate measurement (mL/s) at user-defined cross-sectional planes | P1 |
| FR-014.13 | Time-velocity curve generation across cardiac cycle | P1 |
| FR-014.14 | Stroke volume and regurgitant fraction calculation | P1 |
| FR-014.15 | Pressure gradient estimation (simplified Bernoulli) | P2 |

**FR-014.E: Advanced Hemodynamic Analysis**

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-014.16 | Wall Shear Stress (WSS) calculation on vessel boundaries | P2 |
| FR-014.17 | Oscillatory Shear Index (OSI) computation | P2 |
| FR-014.18 | Turbulent Kinetic Energy (TKE) estimation | P2 |
| FR-014.19 | Vorticity and helicity analysis | P3 |

**FR-014.F: Export and Reporting**

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-014.20 | Export flow quantification results (CSV) | P2 |
| FR-014.21 | Include flow measurements in analysis reports (PDF) | P2 |

### 3.8 Enhanced DICOM IOD Support (P0 - Critical)

#### FR-015: Enhanced Multi-Frame DICOM

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Enhanced DICOM IOD Pipeline                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ┌──────────────┐   ┌──────────────────┐   ┌──────────────────────┐  │
│   │ Enhanced IOD │ → │  Frame Extraction │ → │   Volume Assembly    │  │
│   │   Detection  │   │                  │   │                      │  │
│   │ • CT Enhanced│   │ • Shared Groups  │   │ • Dimension Index    │  │
│   │ • MR Enhanced│   │ • Per-Frame      │   │   based sorting      │  │
│   │ • XA Enhanced│   │   Groups         │   │ • Multi-phase split  │  │
│   └──────────────┘   └──────────────────┘   └──────────────────────┘  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-015.1 | Parse Enhanced CT Image Storage (1.2.840.10008.5.1.4.1.1.2.1) | P0 |
| FR-015.2 | Parse Enhanced MR Image Storage (1.2.840.10008.5.1.4.1.1.4.1) | P0 |
| FR-015.3 | Extract frames from SharedFunctionalGroupsSequence and PerFrameFunctionalGroupsSequence | P0 |
| FR-015.4 | DimensionIndexSequence-based frame ordering and multi-dimensional sorting | P0 |
| FR-015.5 | Backward-compatible with Classic (single-frame) DICOM IOD | P0 |
| FR-015.6 | Support NumberOfFrames-based multi-frame pixel data extraction | P0 |

### 3.9 Cardiac CT Analysis (P1 - High)

#### FR-016: Cardiac CT

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Cardiac CT Pipeline                                    │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ┌──────────────┐   ┌──────────────────┐   ┌──────────────────────┐  │
│   │  ECG-Gated   │ → │ Phase Selection  │ → │   Analysis Tools     │  │
│   │  Detection   │   │                  │   │                      │  │
│   │ • Trigger    │   │ • Best diastole  │   │ • Coronary CTA       │  │
│   │   Time tag   │   │ • Best systole   │   │ • Calcium Score      │  │
│   │ • R-R Window │   │ • All phases     │   │ • Cardiac Function   │  │
│   └──────────────┘   └──────────────────┘   └──────────────────────┘  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**FR-016.A: ECG-Gated Reconstruction**

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-016.1 | Detect ECG-gated cardiac CT series via Trigger Time (0018,1060) and Cardiac Phase tags | P1 |
| FR-016.2 | Separate multi-phase cardiac CT into individual phase volumes | P1 |
| FR-016.3 | Automatic best-phase selection (diastole ~75%, systole ~40%) | P1 |
| FR-016.4 | Multi-phase temporal navigation (reuse TemporalNavigator pattern from 4D Flow) | P1 |

**FR-016.B: Coronary CTA Analysis**

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-016.5 | Coronary artery centerline extraction from contrast-enhanced cardiac CT | P1 |
| FR-016.6 | Curved Planar Reformation (CPR) along extracted centerlines | P1 |
| FR-016.7 | Straightened CPR view for vessel lumen assessment | P2 |
| FR-016.8 | Vessel diameter and stenosis measurement along centerline | P2 |

**FR-016.C: Calcium Scoring**

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-016.9 | Agatston score calculation from non-contrast cardiac CT | P1 |
| FR-016.10 | Per-artery calcium scoring (LAD, LCx, RCA, LM) | P2 |
| FR-016.11 | Volume and mass score alternatives | P2 |
| FR-016.12 | Risk category classification (0, 1-10, 11-100, 101-400, >400) | P1 |

**FR-016.D: Cardiac Function**

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-016.13 | Left ventricular volume calculation (end-diastole and end-systole) | P2 |
| FR-016.14 | Ejection fraction (EF) calculation from multi-phase cardiac CT | P2 |

### 3.10 Cine MRI Support (P2 - Medium)

#### FR-017: Cine MRI

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-017.1 | Detect and classify multi-phase cine MRI series via Trigger Time and Temporal Position Index | P2 |
| FR-017.2 | Temporal navigation with cine playback controls (reuse TemporalNavigator from 4D Flow) | P2 |
| FR-017.3 | Short-axis stack and long-axis (2CH, 3CH, 4CH) view reconstruction | P2 |
| FR-017.4 | Cardiac phase-matched display (synchronize multiple cine series) | P3 |

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
| NFR-016 | 4D Flow loading (≤300 MB) | ≤ 5 seconds |
| NFR-017 | 4D Flow loading (300 MB–2 GB) | ≤ 15 seconds |
| NFR-018 | 4D Flow loading (2–8 GB) | ≤ 45 seconds (streaming) |
| NFR-019 | 4D Flow memory (sliding window) | ≤ 5 phases in RAM simultaneously |
| NFR-020 | 4D Flow streamline rendering | ≥ 15 FPS for ≤ 10K seed points |
| NFR-021 | Enhanced DICOM multi-frame parsing (1000 frames) | ≤ 5 seconds |
| NFR-022 | Cardiac CT multi-phase loading (10 phases) | ≤ 5 seconds |
| NFR-023 | Calcium scoring computation | ≤ 2 seconds |
| NFR-024 | Coronary centerline extraction | ≤ 10 seconds |

### 4.2 Compatibility

| ID | Requirement | Details |
|----|-------------|---------|
| NFR-006 | Operating System | macOS 12+, Windows 10+, Ubuntu 20.04+ |
| NFR-007 | Graphics Card | GPU supporting OpenGL 4.1+ |
| NFR-008 | DICOM Standard | DICOM PS3.x compliant (Classic and Enhanced IODs) |
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
│   │                    └─ ToolController (Tool control)              │  │
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
│   Phase 1: Foundation    Phase 2: Core     Phase 3: Enhance  Phase 4: Flow│
│   ─────────────────      ──────────────   ────────────────   ────────────│
│                                                                         │
│   v0.1 ─┬─ v0.2 ─┬─ v0.3 ─┬─ v0.4 ─┬─ v0.5 ─┬─ v0.6 ─┬─ v1.0        │
│   (Pre) │ (Pre)  │ (MVP)  │ (Core) │ (Enh)  │ (Flow) │ (Release)     │
│   ┌─────┘  ┌─────┘  ┌─────┘  ┌─────┘  ┌─────┘  ┌─────┘               │
│   │        │        │        │        │        │                       │
│   Wk 1-2   Wk 3-4   Wk 5-8   Wk 9-12  Wk 13-16 Wk 17-24             │
│                                                                         │
│   • Project • DICOM  • MPR    • PACS   • DR/CR  • 4D Flow             │
│     setup     load   • Volume   integr.• Adv.     DICOM               │
│   • Basic  • ITK/     render.• Measure   segm.  • Flow viz            │
│     UI       VTK    • Surface• Basic   • Quant.  • Quantify           │
│              integr.   render.  segm.    analysis• WSS/TKE            │
│                      • Presets         • Reports                       │
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

#### Milestone 5: 4D Flow MRI Analysis (v0.6.0) - Week 17-24

| Item | Description | Completion Criteria |
|------|-------------|---------------------|
| M5.1 | 4D Flow DICOM parsing | Vendor-specific (Siemens, Philips, GE) parsing verified |
| M5.2 | Velocity field assembly | Vector field construction with VENC scaling |
| M5.3 | Phase correction | Aliasing unwrap, eddy current correction |
| M5.4 | Temporal navigation | Cardiac phase slider, cine playback at ≥15 fps |
| M5.5 | Flow visualization | Streamlines, pathlines, vector glyphs |
| M5.6 | Flow quantification | Flow rate at cross-section, time-velocity curves |
| M5.7 | Advanced analysis | WSS, OSI, TKE on vessel boundaries |
| M5.8 | Flow export and reporting | CSV export of flow results, PDF report integration |
| M5.9 | Integration testing | End-to-end validation with analytical ground truth |

#### Milestone 6: Enhanced DICOM & Cardiac CT (v0.7.0) - Week 25-32

| Item | Description | Completion Criteria |
|------|-------------|---------------------|
| M6.1 | Enhanced DICOM IOD parser | Multi-frame CT/MR Enhanced IOD parsing verified |
| M6.2 | Frame extraction and sorting | DimensionIndexSequence-based ordering correct |
| M6.3 | ECG-gated phase separation | Cardiac CT phases correctly split by trigger time |
| M6.4 | Coronary CTA centerline | Centerline extraction and CPR view functional |
| M6.5 | Calcium scoring | Agatston score with per-artery breakdown |
| M6.6 | Cine MRI temporal display | Multi-phase cine playback at ≥15 fps |
| M6.7 | Integration testing | End-to-end validation with cardiac CT/MRI datasets |

---

## 8. Risks & Mitigations

### 8.1 Technical Risks

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| GPU compatibility issues | High | Medium | CPU fallback with vtkSmartVolumeMapper |
| Memory shortage for large data | High | Medium | Streaming loading, LOD implementation |
| ITK-VTK version compatibility | Medium | Low | Version pinning with vcpkg |
| Compression format decoding failure | Medium | Low | Pre-validate pacs_system codecs |
| 4D Flow vendor DICOM variability | High | High | Test with datasets from all 3 major vendors |
| 4D Flow memory pressure (>1 GB datasets) | High | Medium | Sliding window cache, memory-mapped I/O |

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
| CT | CT | P0 | Yes | Volume rendering, MPR, HU, Cardiac CT (ECG-gated, CTA, Calcium Score) |
| MRI | MR | P0 | Yes | Volume rendering, MPR, multi-sequence, 4D Flow (velocity-encoded), Cine MRI |
| Enhanced CT | CT (Enhanced) | P0 | Yes | Multi-frame IOD, per-frame functional groups |
| Enhanced MR | MR (Enhanced) | P0 | Yes | Multi-frame IOD, per-frame functional groups |
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
| 0.3.0 | 2026-02-11 | Development Team | Replaced DCMTK with pacs_system for DICOM network operations; version sync with build system |
| 0.4.0 | 2026-02-11 | Development Team | Added FR-014 (4D Flow MRI Analysis) with 21 sub-requirements; added NFR-016~020 for tiered performance targets; added UC-11, UC-12; added Milestone 5 (v0.6.0) |
| 0.5.0 | 2026-02-12 | Development Team | Added FR-015 (Enhanced DICOM IOD, 6 sub-reqs), FR-016 (Cardiac CT, 14 sub-reqs), FR-017 (Cine MRI, 4 sub-reqs); added NFR-021~024; added UC-13~16; added Milestone 6 (v0.7.0) |

> **Note**: v0.x.x are Pre-release versions. Official releases start from v1.0.0.

---

*This document is subject to change based on stakeholder feedback and technical discoveries during development.*

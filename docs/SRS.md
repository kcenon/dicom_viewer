# DICOM Viewer - Software Requirements Specification (SRS)

> **Version**: 0.3.0
> **Created**: 2025-12-31
> **Last Updated**: 2026-02-11
> **Status**: Draft (Pre-release)
> **Author**: Development Team
> **Based on**: [PRD v0.3.0](PRD.md)

---

## Document Information

### Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 0.1.0 | 2025-12-31 | Development Team | Initial SRS based on PRD 0.1.0 |
| 0.2.0 | 2025-12-31 | Development Team | Added segmentation and measurement requirements (FR-006, FR-007 expansion) |
| 0.3.0 | 2026-02-11 | Development Team | Replaced DCMTK with pacs_system for DICOM network operations; version sync with build system |

### Referenced Documents

| Document ID | Title | Location |
|-------------|-------|----------|
| PRD-001 | Product Requirements Document | [PRD.md](PRD.md) |
| REF-001 | ITK Overview | [reference/01-itk-overview.md](reference/01-itk-overview.md) |
| REF-002 | VTK Overview | [reference/02-vtk-overview.md](reference/02-vtk-overview.md) |
| REF-003 | ITK-VTK Integration | [reference/03-itk-vtk-integration.md](reference/03-itk-vtk-integration.md) |
| REF-004 | DICOM Pipeline | [reference/04-dicom-pipeline.md](reference/04-dicom-pipeline.md) |
| REF-005 | PACS Integration | [reference/05-pacs-integration.md](reference/05-pacs-integration.md) |
| REF-006 | GUI Framework Comparison | [reference/06-gui-framework-comparison.md](reference/06-gui-framework-comparison.md) |
| REF-007 | Remote Visualization | [reference/07-remote-visualization.md](reference/07-remote-visualization.md) |

### Requirement ID Convention

- **SRS-FR-XXX**: Functional Requirements
- **SRS-NFR-XXX**: Non-Functional Requirements
- **SRS-IF-XXX**: Interface Requirements
- **SRS-DR-XXX**: Data Requirements
- **SRS-CR-XXX**: Constraint Requirements

---

## 1. Introduction

### 1.1 Purpose

This document is the detailed Software Requirements Specification (SRS) for the DICOM Viewer software. It transforms product requirements defined in the PRD into technical software requirements and provides detailed specifications necessary for implementation.

### 1.2 Scope

DICOM Viewer is a desktop application for medical imaging (CT, MRI, DR/CR) that provides:
- CT/MRI 3D volume rendering and MPR views
- Region segmentation and measurement
- DR/CR 2D viewing
- PACS integration

### 1.3 Definitions, Acronyms, and Abbreviations

| Term | Definition |
|------|------------|
| **DICOM** | Digital Imaging and Communications in Medicine - Medical imaging standard |
| **HU** | Hounsfield Unit - CT image density measurement unit |
| **MPR** | Multi-Planar Reconstruction |
| **PACS** | Picture Archiving and Communication System |
| **ROI** | Region of Interest |
| **VR** | Volume Rendering |
| **LPS** | Left-Posterior-Superior coordinate system |
| **Transfer Syntax** | DICOM data encoding method |
| **SCP** | Service Class Provider - DICOM service provider |
| **SCU** | Service Class User - DICOM service user |

### 1.4 System Overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    DICOM Viewer System Architecture                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐  │
│   │                          UI Layer                                │  │
│   │                                                                   │  │
│   │    Qt6 Widgets + QVTKOpenGLNativeWidget                          │  │
│   │    • MainWindow, ViewportManager, ToolsPanel, StatusBar          │  │
│   │                                                                   │  │
│   └─────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│   ┌─────────────────────────────────────────────────────────────────┐  │
│   │                     Controller Layer                             │  │
│   │                                                                   │  │
│   │    ViewerController → LoadingController, RenderingController,    │  │
│   │                       ToolController, NetworkController          │  │
│   │                                                                   │  │
│   └─────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│   ┌─────────────────────────────────────────────────────────────────┐  │
│   │                       Service Layer                              │  │
│   │                                                                   │  │
│   │  ┌────────────────┐  ┌────────────────┐  ┌───────────────────┐  │  │
│   │  │ ImageService   │  │ RenderService  │  │  NetworkService   │  │  │
│   │  │ (ITK-based)    │  │ (VTK-based)    │  │  (pacs_system)    │  │  │
│   │  └────────────────┘  └────────────────┘  └───────────────────┘  │  │
│   │                                                                   │  │
│   └─────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│   ┌─────────────────────────────────────────────────────────────────┐  │
│   │                        Data Layer                                │  │
│   │                                                                   │  │
│   │    pacs_system (DICOM Core) + ITK (Processing) + VTK (Render)   │  │
│   │                                                                   │  │
│   └─────────────────────────────────────────────────────────────────┘  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Functional Requirements

### 2.1 DICOM Series Loading (CT/MRI)

#### SRS-FR-001: DICOM File Parsing
**Traces to**: PRD FR-001.1, FR-001.4

| Attribute | Value |
|-----------|-------|
| Priority | P0 (Critical) |
| Source | PRD FR-001 |
| Dependency | pacs_system/core |

**Description**: The system shall read and parse DICOM files from the local file system.

**Specification**:
- Use pacs_system's `dicom_file::open()` API for DICOM file parsing
- Supported DICOM Part 10 format: Preamble(128 bytes) + "DICM" Prefix + Meta Information + Dataset
- Required metadata extraction:
  - Patient Module: (0010,0010) Patient Name, (0010,0020) Patient ID
  - Study Module: (0020,000D) Study Instance UID, (0008,0020) Study Date
  - Series Module: (0020,000E) Series Instance UID, (0008,0060) Modality
  - Image Module: (0028,0010) Rows, (0028,0011) Columns, (0028,0100) Bits Allocated

**Input**:
- File path (std::filesystem::path)

**Output**:
- pacs::dicom_file object or error code

**Verification**:
- Return success when loading valid DICOM file
- Return clear error message when loading invalid file

---

#### SRS-FR-002: DICOM Series Assembly
**Traces to**: PRD FR-001.2

| Attribute | Value |
|-----------|-------|
| Priority | P0 (Critical) |
| Source | PRD FR-001 |
| Dependency | SRS-FR-001 |

**Description**: The system shall automatically detect DICOM series from a directory and assemble them into a 3D volume.

**Specification**:
- Scan directory to detect all DICOM files
- Group by Series Instance UID (0020,000E)
- Slice sorting algorithm:
  1. **Primary**: Sort by Z coordinate of Image Position Patient (0020,0032)
  2. **Fallback 1**: Sort by Slice Location (0020,1041)
  3. **Fallback 2**: Sort by Instance Number (0020,0013)
- Convert sorted slices to ITK Image

**Algorithm**:
```cpp
struct SliceInfo {
    std::filesystem::path path;
    double sliceLocation;       // (0020,1041) or computed from (0020,0032)
    int instanceNumber;         // (0020,0013)
    std::array<double, 3> imagePosition; // (0020,0032)
};

// Sort by slice location (ascending)
std::sort(slices.begin(), slices.end(),
    [](const SliceInfo& a, const SliceInfo& b) {
        return a.sliceLocation < b.sliceLocation;
    });
```

**Input**:
- Directory path

**Output**:
- `itk::Image<short, 3>` (CT) or `itk::Image<unsigned short, 3>` (MRI)

---

#### SRS-FR-003: Transfer Syntax Decoding
**Traces to**: PRD FR-001.3

| Attribute | Value |
|-----------|-------|
| Priority | P0 (Critical) |
| Source | PRD FR-001 |
| Dependency | pacs_system/encoding |

**Description**: The system shall automatically decode compressed data from various DICOM Transfer Syntaxes.

**Specification**:

| Transfer Syntax UID | Name | Support |
|---------------------|------|---------|
| 1.2.840.10008.1.2 | Implicit VR Little Endian | Required |
| 1.2.840.10008.1.2.1 | Explicit VR Little Endian | Required |
| 1.2.840.10008.1.2.4.50 | JPEG Baseline (Process 1) | Required |
| 1.2.840.10008.1.2.4.70 | JPEG Lossless, Non-Hierarchical | Required |
| 1.2.840.10008.1.2.4.90 | JPEG 2000 Image Compression (Lossless Only) | Required |
| 1.2.840.10008.1.2.4.91 | JPEG 2000 Image Compression | Required |
| 1.2.840.10008.1.2.4.80 | JPEG-LS Lossless | Required |
| 1.2.840.10008.1.2.5 | RLE Lossless | Required |

**Implementation**:
- Use pacs_system's `codec_factory::create(transferSyntaxUID)`
- Determine compression method from Transfer Syntax UID (0002,0010)
- Automatic codec selection and decoding

---

#### SRS-FR-004: Hounsfield Unit Conversion
**Traces to**: PRD FR-001.5

| Attribute | Value |
|-----------|-------|
| Priority | P0 (Critical) |
| Source | PRD FR-001 |
| Dependency | SRS-FR-001 |

**Description**: CT image stored pixel values shall be converted to Hounsfield Units.

**Specification**:
```
HU = StoredValue × RescaleSlope + RescaleIntercept
```

- **Rescale Slope**: (0028,1053), default 1.0
- **Rescale Intercept**: (0028,1052), default 0.0

**Reference HU Values** (Ref: REF-004):

| Tissue | HU Range |
|--------|----------|
| Air | -1000 |
| Lung | -500 ~ -900 |
| Fat | -100 ~ -50 |
| Water | 0 |
| Soft Tissue | 10 ~ 80 |
| Bone (Cancellous) | 100 ~ 300 |
| Bone (Cortical) | 300 ~ 3000 |

---

### 2.2 Volume Rendering

#### SRS-FR-005: GPU-Accelerated Volume Rendering
**Traces to**: PRD FR-002.1

| Attribute | Value |
|-----------|-------|
| Priority | P0 (Critical) |
| Source | PRD FR-002 |
| Dependency | VTK RenderingVolumeOpenGL2 |

**Description**: The system shall perform volume rendering using GPU-accelerated Ray Casting.

**Specification**:
- **Primary Mapper**: `vtkGPUVolumeRayCastMapper`
- **Fallback Mapper**: `vtkSmartVolumeMapper` (fallback to CPU when GPU not supported)
- Blend Mode: Composite (default), Maximum Intensity Projection (MIP)
- Sampling Distance: Automatic (based on data spacing)

**Performance Requirements**:
- 512×512×300 volume: 30 FPS or higher (Ref: PRD NFR-002)
- Interactive rendering: Apply LOD (Level of Detail)

**Implementation** (Ref: REF-002):
```cpp
auto volumeMapper = vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
volumeMapper->SetInputConnection(imageData->GetOutputPort());
volumeMapper->SetBlendModeToComposite();

// GPU support check and fallback
if (!volumeMapper->IsRenderSupported(renderWindow, volumeProperty)) {
    auto smartMapper = vtkSmartPointer<vtkSmartVolumeMapper>::New();
    // ... CPU fallback setup
}
```

---

#### SRS-FR-006: Transfer Function Management
**Traces to**: PRD FR-002.2, FR-002.3, FR-002.4

| Attribute | Value |
|-----------|-------|
| Priority | P0 (Critical) |
| Source | PRD FR-002 |
| Dependency | SRS-FR-005 |

**Description**: The system shall manage Color/Opacity Transfer Functions for volume rendering.

**Specification**:

**Color Transfer Function** (`vtkColorTransferFunction`):
- Add/modify/delete RGB color mapping points
- Linear interpolation (default) or step interpolation

**Opacity Transfer Function** (`vtkPiecewiseFunction`):
- Scalar value → opacity mapping
- Gradient Opacity support (edge enhancement)

**Built-in Presets** (Ref: PRD FR-002.4):

| Preset | Window Width | Window Center | Color Scheme |
|--------|--------------|---------------|--------------|
| CT Bone | 2000 | 400 | White/Ivory |
| CT Soft Tissue | 400 | 40 | Skin tones |
| CT Lung | 1500 | -600 | Gray scale |
| CT Angio | 400 | 200 | Red (vessels) |
| CT Abdomen | 400 | 50 | Multi-tissue |
| MRI Default | Auto | Auto | Gray scale |

**Data Structure**:
```cpp
struct TransferFunctionPreset {
    std::string name;
    std::vector<std::tuple<double, double, double, double>> colorPoints; // (value, r, g, b)
    std::vector<std::pair<double, double>> opacityPoints; // (value, opacity)
    double windowWidth;
    double windowCenter;
};
```

---

#### SRS-FR-007: Volume Clipping
**Traces to**: PRD FR-002.6

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-002 |
| Dependency | SRS-FR-005 |

**Description**: The system shall support clipping (cutting) part of the volume to view internal structures.

**Specification**:
- `vtkBoxWidget2` based interactive clipping box
- Individual adjustment of 6-plane clipping
- Toggle inside/outside clipping region

---

### 2.3 Multi-Planar Reconstruction (MPR)

#### SRS-FR-008: Orthogonal MPR Views
**Traces to**: PRD FR-003.1, FR-003.2, FR-003.3

| Attribute | Value |
|-----------|-------|
| Priority | P0 (Critical) |
| Source | PRD FR-003 |
| Dependency | VTK ImagingCore |

**Description**: The system shall display three orthogonal plane views (Axial, Coronal, Sagittal) simultaneously.

**Specification**:

**Reslice Matrix Definitions** (Ref: REF-002):

| View | Orientation | vtkImageReslice Matrix |
|------|-------------|------------------------|
| Axial (XY) | Top-down | Identity + Z translation |
| Coronal (XZ) | Front | Rotate -90° around X axis |
| Sagittal (YZ) | Side | Rotate 90° around Y axis |

**Features**:
- Mouse wheel: Slice scroll
- Mouse drag (Left): Window/Level adjustment
- Mouse drag (Right): Pan
- Mouse drag (Middle): Zoom

**Crosshair Synchronization**:
- Click in one view updates crosshairs in other views
- Physical coordinate-based synchronization (LPS coordinate system)

**Implementation** (Ref: REF-002):
```cpp
// Coronal reslice matrix
auto coronalMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
coronalMatrix->DeepCopy({
    1, 0, 0, 0,
    0, 0, 1, 0,
    0, -1, 0, slicePosition,
    0, 0, 0, 1
});
reslice->SetResliceAxes(coronalMatrix);
```

---

#### SRS-FR-009: Window/Level Adjustment
**Traces to**: PRD FR-003.4

| Attribute | Value |
|-----------|-------|
| Priority | P0 (Critical) |
| Source | PRD FR-003 |
| Dependency | SRS-FR-008 |

**Description**: The system shall allow real-time Window/Level adjustment via mouse drag.

**Specification**:
- **Window Width (W)**: Width of displayed HU range
- **Window Level/Center (L)**: Center value of displayed HU range
- **Display Range**: [L - W/2, L + W/2]

**Mouse Interaction**:
- X direction drag: Adjust Window Width
- Y direction drag: Adjust Window Level
- Sensitivity: 1 pixel = 1 HU unit

**Presets** (2D Viewing):

| Preset | Window Width | Window Center | Use |
|--------|--------------|---------------|-----|
| Chest | 1500 | -600 | Chest X-ray |
| Abdomen | 400 | 40 | Abdomen |
| Bone | 2000 | 300 | Skeleton |
| Brain | 80 | 40 | Brain CT |
| Liver | 150 | 30 | Liver |
| Mediastinum | 350 | 50 | Mediastinum |

---

#### SRS-FR-010: Oblique Reslicing
**Traces to**: PRD FR-003.5

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-003 |
| Dependency | SRS-FR-008 |

**Description**: The system shall extract slices from arbitrarily angled planes.

**Specification**:
- Use `vtkImagePlaneWidget` or `vtkImageReslice`
- Rotation angle: -180° ~ +180° (all axes)
- Plane definition: Center point + normal vector
- Real-time interactive manipulation

---

#### SRS-FR-011: Thick Slab Reconstruction
**Traces to**: PRD FR-003.6

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-003 |
| Dependency | SRS-FR-008 |

**Description**: The system shall generate Thick Slab images by combining multiple slices.

**Specification**:

| Mode | Algorithm | Use Case |
|------|-----------|----------|
| MIP | Maximum Intensity Projection | Vascular imaging |
| MinIP | Minimum Intensity Projection | Airway visualization |
| AverageIP | Average Intensity Projection | Noise reduction |

**Parameters**:
- Slab Thickness: 1mm ~ 50mm (user configurable)
- Slab Type: MIP, MinIP, Average

---

### 2.4 Surface Rendering

#### SRS-FR-012: Isosurface Extraction
**Traces to**: PRD FR-004.1, FR-004.2

| Attribute | Value |
|-----------|-------|
| Priority | P0 (Critical) |
| Source | PRD FR-004 |
| Dependency | VTK FiltersCore |

**Description**: The system shall extract isosurfaces using the Marching Cubes algorithm.

**Specification** (Ref: REF-002):
- **Algorithm**: `vtkMarchingCubes` or `vtkContourFilter`
- **Input**: `vtkImageData` (3D volume)
- **Output**: `vtkPolyData` (triangle mesh)
- **Threshold**: User-specified isovalue (HU-based for CT)

**Default Thresholds**:

| Tissue | Isovalue (HU) |
|--------|---------------|
| Bone | 200 ~ 400 |
| Soft Tissue | 40 ~ 80 |
| Skin | -100 ~ 0 |

---

#### SRS-FR-013: Surface Smoothing and Decimation
**Traces to**: PRD FR-004.3

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-004 |
| Dependency | SRS-FR-012 |

**Description**: The system shall smooth extracted surfaces and reduce polygon count to improve rendering performance.

**Specification**:

**Smoothing**:
- `vtkWindowedSincPolyDataFilter` (recommended) or `vtkSmoothPolyDataFilter`
- Iterations: 10 ~ 50 (user configurable)
- Pass Band: 0.05 ~ 0.2

**Decimation**:
- `vtkDecimatePro` or `vtkQuadricDecimation`
- Target Reduction: 0.3 ~ 0.8 (30% ~ 80% reduction)
- Preserve Topology: On

---

#### SRS-FR-014: Multi-Surface Rendering
**Traces to**: PRD FR-004.4

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-004 |
| Dependency | SRS-FR-012 |

**Description**: The system shall render multiple tissue surfaces simultaneously with different colors.

**Specification**:
- Create separate `vtkActor` for each surface
- Individual color, transparency, specular properties
- Show/hide toggle per surface

---

#### SRS-FR-015: Mesh Export
**Traces to**: PRD FR-004.5

| Attribute | Value |
|-----------|-------|
| Priority | P2 (Medium) |
| Source | PRD FR-004 |
| Dependency | SRS-FR-012 |

**Description**: The system shall export extracted surface meshes to external formats.

**Supported Formats**:

| Format | Extension | Writer Class | Use Case |
|--------|-----------|--------------|----------|
| STL (Binary) | .stl | `vtkSTLWriter` | 3D printing |
| STL (ASCII) | .stl | `vtkSTLWriter` | Compatibility |
| PLY | .ply | `vtkPLYWriter` | With color |
| OBJ | .obj | `vtkOBJWriter` | General purpose |

---

### 2.5 Image Preprocessing

#### SRS-FR-016: Gaussian Smoothing
**Traces to**: PRD FR-005.1

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-005 |
| Dependency | ITK Smoothing |

**Description**: The system shall apply Gaussian filter to remove noise.

**Specification** (Ref: REF-001):
- **Filter**: `itk::DiscreteGaussianImageFilter`
- **Parameter**: Variance (σ²), default 1.0
- **Kernel Width**: Automatic (default) or user specified (max 32)

**Input**: `itk::Image<short, 3>`
**Output**: `itk::Image<short, 3>` (same type)

---

#### SRS-FR-017: Anisotropic Diffusion
**Traces to**: PRD FR-005.2

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-005 |
| Dependency | ITK Smoothing |

**Description**: The system shall apply anisotropic diffusion filter that removes noise while preserving edges.

**Specification** (Ref: REF-001):
- **Filter**: `itk::CurvatureAnisotropicDiffusionImageFilter`
- **Parameters**:
  - Time Step: 0.0625 (3D stability condition)
  - Iterations: 5 ~ 20
  - Conductance: 1.0 ~ 5.0

---

#### SRS-FR-018: N4 Bias Field Correction
**Traces to**: PRD FR-005.4

| Attribute | Value |
|-----------|-------|
| Priority | P2 (Medium) |
| Source | PRD FR-005 |
| Dependency | ITK BiasCorrection |

**Description**: The system shall correct non-uniform intensity distribution in MRI images.

**Specification** (Ref: REF-004):
- **Filter**: `itk::N4BiasFieldCorrectionImageFilter`
- **Parameters**:
  - Fitting Levels: 4 (default)
  - Maximum Iterations: [50, 50, 50, 50]
  - Convergence Threshold: 0.001

**Input**: `itk::Image<float, 3>` + mask (optional)
**Output**: Corrected `itk::Image<float, 3>`

---

#### SRS-FR-019: Isotropic Resampling
**Traces to**: PRD FR-005.5

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-005 |
| Dependency | ITK ImageFunction |

**Description**: The system shall resample anisotropic voxels to isotropic voxels.

**Specification** (Ref: REF-004):
- **Filter**: `itk::ResampleImageFilter`
- **Interpolator**: `itk::LinearInterpolateImageFunction` (default)
- **Target Spacing**: User specified (e.g., 1.0mm isotropic)

**Algorithm**:
```cpp
// Calculate new size
for (int i = 0; i < 3; ++i) {
    outputSize[i] = static_cast<unsigned int>(
        inputSize[i] * inputSpacing[i] / targetSpacing
    );
}
```

---

### 2.6 Segmentation

#### SRS-FR-020: Threshold Segmentation
**Traces to**: PRD FR-006.1, FR-006.2

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-006 |
| Dependency | ITK Thresholding |

**Description**: The system shall segment regions based on HU value range.

**Specification** (Ref: REF-001, REF-004):

**Manual Threshold**:
- **Filter**: `itk::BinaryThresholdImageFilter`
- **Parameters**: Lower, Upper threshold (HU)
- **Output**: Binary mask (`itk::Image<unsigned char, 3>`)

**Otsu Automatic Threshold**:
- **Filter**: `itk::OtsuThresholdImageFilter`
- **Multi-class**: `itk::OtsuMultipleThresholdsImageFilter`

**Preset Thresholds** (CT):

| Target | Lower HU | Upper HU | Algorithm |
|--------|----------|----------|-----------|
| Bone | 200 | 3000 | Threshold + Morphology |
| Soft Tissue | -100 | 200 | Region Growing |
| Lung | -950 | -400 | Threshold + Hole Fill |
| Liver | 40 | 80 | Region Growing |
| Vessel (Contrast) | 150 | 500 | Threshold |
| Fat | -150 | -50 | Threshold |

---

#### SRS-FR-021: Region Growing Segmentation
**Traces to**: PRD FR-006.3, FR-006.4

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-006 |
| Dependency | ITK RegionGrowing |

**Description**: The system shall segment using seed point-based region growing algorithm.

**Specification** (Ref: REF-001):

**Connected Threshold**:
- **Filter**: `itk::ConnectedThresholdImageFilter`
- **Input**: Seed point (3D index), Lower/Upper threshold
- **Output**: Binary mask

**Confidence Connected**:
- **Filter**: `itk::ConfidenceConnectedImageFilter`
- **Parameters**:
  - Multiplier: Standard deviation multiplier (default 2.5)
  - Iterations: 5 (default)
  - Initial Neighborhood Radius: 2 (default)

**User Interaction**:
- Click in MPR view to set seed point
- Physical coordinates → index coordinates conversion required

---

#### SRS-FR-022: Level Set Segmentation
**Traces to**: PRD FR-006.5

| Attribute | Value |
|-----------|-------|
| Priority | P2 (Medium) |
| Source | PRD FR-006 |
| Dependency | ITK LevelSets |

**Description**: The system shall apply Level Set-based segmentation algorithm.

**Specification** (Ref: REF-001):
- **Pipeline**:
  1. Gradient Magnitude (`itk::GradientMagnitudeRecursiveGaussianImageFilter`)
  2. Sigmoid Feature (`itk::SigmoidImageFilter`)
  3. Fast Marching (Initial Level Set) (`itk::FastMarchingImageFilter`)
  4. Geodesic Active Contour (`itk::GeodesicActiveContourLevelSetImageFilter`)
  5. Binary Thresholding

**Parameters**:
- Propagation Scaling: 1.0 (default)
- Curvature Scaling: 1.0 (default)
- Advection Scaling: 1.0 (default)
- Maximum RMS Error: 0.02
- Maximum Iterations: 800

---

#### SRS-FR-023: Manual Segmentation Tools
**Traces to**: PRD FR-006.7 ~ FR-006.12

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-006 |
| Dependency | VTK Widgets, Qt |

**Description**: The system shall provide drawing tools for manual segmentation.

**Tools**:

| Tool | Description | Interaction |
|------|-------------|-------------|
| **Brush** | Draw with circular/square brush | Click & Drag |
| **Eraser** | Remove segmentation region | Click & Drag |
| **Fill** | Fill closed region | Click |
| **Freehand** | Draw freehand curve | Click & Drag |
| **Polygon** | Polygon ROI | Click (add vertex) + Double-click (complete) |
| **Smart Scissors** | Edge tracking (LiveWire) | Click (anchor) + Move (track) |

**Brush Parameters**:
- Size: 1px ~ 50px
- Shape: Circle, Square

---

#### SRS-FR-024: Multi-Label Segmentation
**Traces to**: PRD FR-006.13 ~ FR-006.18

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-006 |
| Dependency | SRS-FR-020 ~ SRS-FR-023 |

**Description**: The system shall segment and manage multiple organs/tissues simultaneously.

**Specification**:

**Label Management**:
- Label ID: 1 ~ 255 (0 is background)
- Label properties: name, color (RGBA), visibility
- Active label selection (current editing target)

**Data Structure**:
```cpp
struct SegmentationLabel {
    uint8_t id;
    std::string name;
    std::array<uint8_t, 4> color; // RGBA
    bool visible;
};
```

**Storage Format**:
- NRRD (.nrrd) - recommended
- NIfTI (.nii, .nii.gz)
- ITK MetaImage (.mha, .mhd)

---

#### SRS-FR-025: Morphological Post-Processing
**Traces to**: PRD FR-006.19 ~ FR-006.25

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-006 |
| Dependency | ITK MathematicalMorphology |

**Description**: The system shall apply morphological operations to segmentation results.

**Operations** (Ref: REF-004):

| Operation | Filter | Effect |
|-----------|--------|--------|
| **Opening** | `itk::BinaryMorphologicalOpeningImageFilter` | Remove small protrusions |
| **Closing** | `itk::BinaryMorphologicalClosingImageFilter` | Fill small holes |
| **Dilation** | `itk::BinaryDilateImageFilter` | Expand region |
| **Erosion** | `itk::BinaryErodeImageFilter` | Shrink region |
| **Fill Holes** | `itk::BinaryFillholeImageFilter` | Fill internal holes |
| **Island Removal** | `itk::BinaryShapeKeepNObjectsImageFilter` | Remove small connected components |

**Structuring Element**:
- Type: Ball (sphere in 3D)
- Radius: 1 ~ 10 voxels (user configurable)

---

### 2.7 Measurement

#### SRS-FR-026: Linear Measurement
**Traces to**: PRD FR-007.1 ~ FR-007.5

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-007 |
| Dependency | VTK Widgets |

**Description**: The system shall measure distance between two points.

**Specification**:

**Distance Measurement**:
- Widget: `vtkDistanceWidget` or `vtkLineWidget2`
- Unit: mm (based on DICOM Pixel Spacing)
- Precision: 2 decimal places

**Angle Measurement**:
- Widget: `vtkAngleWidget`
- Unit: degrees (°)
- Specify 3 points (vertex is the angle point)

**Annotation Display**:
- Overlay measurement results on screen
- User configurable font size and color

---

#### SRS-FR-027: Area Measurement (ROI)
**Traces to**: PRD FR-007.6 ~ FR-007.10

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-007 |
| Dependency | VTK Widgets |

**Description**: The system shall measure area inside 2D ROI.

**ROI Types**:

| Type | Widget | Calculation |
|------|--------|-------------|
| Ellipse | `vtkEllipseWidget` | π × a × b |
| Rectangle | `vtkBorderWidget` | width × height |
| Polygon | `vtkContourWidget` | Shoelace formula |
| Freehand | `vtkContourWidget` | Polygon approximation |

**Output**:
- Area: mm²
- Perimeter: mm

---

#### SRS-FR-028: ROI Statistics
**Traces to**: PRD FR-007.15 ~ FR-007.20

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-007 |
| Dependency | ITK LabelStatistics |

**Description**: The system shall calculate statistics within ROI or segmented regions.

**Statistics** (Ref: REF-004):
- **Mean**: Average HU/signal intensity
- **Standard Deviation**: Standard deviation
- **Minimum/Maximum**: Min/max values
- **Median**: Median value
- **Pixel/Voxel Count**: Number of pixels/voxels

**Implementation**:
```cpp
using LabelStatisticsFilterType = itk::LabelStatisticsImageFilter<
    ImageType, MaskImageType>;
auto labelStatistics = LabelStatisticsFilterType::New();
labelStatistics->SetInput(image);
labelStatistics->SetLabelInput(labelImage);
labelStatistics->Update();

double mean = labelStatistics->GetMean(labelValue);
double stdDev = labelStatistics->GetSigma(labelValue);
```

**Histogram**:
- Display histogram (Qt Charts or VTK Charts)
- Bin count: automatic or user specified

---

#### SRS-FR-029: Volume Measurement
**Traces to**: PRD FR-007.11 ~ FR-007.14

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-007 |
| Dependency | SRS-FR-024 |

**Description**: The system shall calculate 3D volume of segmented regions.

**Calculation** (Ref: REF-004):
```
Volume (mm³) = Voxel Count × (Spacing[0] × Spacing[1] × Spacing[2])
Volume (cm³) = Volume (mm³) / 1000
Volume (mL) = Volume (cm³)  // 1 cm³ = 1 mL
```

**Output**:
- Volume: mm³, cm³, mL
- For multiple labels: Volume comparison table per label

---

#### SRS-FR-030: Advanced Quantitative Analysis
**Traces to**: PRD FR-007.21 ~ FR-007.25

| Attribute | Value |
|-----------|-------|
| Priority | P2 (Medium) |
| Source | PRD FR-007 |
| Dependency | ITK ShapeLabelMap |

**Description**: The system shall analyze advanced morphological characteristics of segmented regions.

**Metrics** (Ref: REF-004):

| Metric | Description | Formula |
|--------|-------------|---------|
| Surface Area | Surface area (mm²) | Marching Cubes → mesh area |
| Sphericity | Sphericity | (π^(1/3) × (6V)^(2/3)) / A |
| Elongation | Elongation ratio | (major axis) / (minor axis) |
| Centroid | Center of mass | Σ(position × intensity) / Σ(intensity) |
| Principal Axes | Principal axis directions | PCA on voxel positions |

**Implementation**:
- Use `itk::ShapeLabelMap` and `itk::BinaryImageToShapeLabelMapFilter`

---

### 2.8 ROI Management

#### SRS-FR-031: ROI List Management
**Traces to**: PRD FR-012.1 ~ FR-012.8

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-012 |
| Dependency | Qt Widgets |

**Description**: The system shall manage all created ROIs in a list.

**Features**:
- ROI list panel (TreeView or TableView)
- Edit ROI name (double-click)
- Change ROI color (Color picker)
- Show/hide toggle (checkbox)
- Delete ROI (Delete key or button)
- Copy/paste ROI (to other slices)

**ROI Serialization Formats**:

| Format | Extension | Use Case |
|--------|-----------|----------|
| JSON | .json | General purpose, editable |
| CSV | .csv | Spreadsheet compatible |
| DICOM SR | .dcm | PACS storage |

---

### 2.9 Analysis Report

#### SRS-FR-032: Report Generation
**Traces to**: PRD FR-013.1 ~ FR-013.6

| Attribute | Value |
|-----------|-------|
| Priority | P2 (Medium) |
| Source | PRD FR-013 |
| Dependency | Qt Print Support |

**Description**: The system shall generate documented reports of analysis results.

**Report Contents**:
1. **Header**: Patient information, exam date, modality
2. **Segmentation Summary**: Volume, mean values, surface area per label
3. **Measurement Results**: List of ROI measurements
4. **Screenshots**: Key view screenshots

**Export Formats**:

| Format | Library | Features |
|--------|---------|----------|
| PDF | Qt PDF Writer / libharu | Print optimized |
| CSV/Excel | Qt CSV / libxlsx | Data analysis |
| DICOM SR | pacs_system | PACS storage |

---

### 2.10 DR/CR 2D Viewing

#### SRS-FR-033: 2D Image Display
**Traces to**: PRD FR-008.1 ~ FR-008.6

| Attribute | Value |
|-----------|-------|
| Priority | P2 (Medium) |
| Source | PRD FR-008 |
| Dependency | VTK ImagingCore |

**Description**: The system shall display single-frame DICOM images (DR, CR) in 2D.

**Features**:
- `vtkImageViewer2` based 2D viewer
- Window/Level adjustment (mouse drag)
- Zoom/Pan (mouse wheel, drag)
- Rotation (90° steps, free rotation)
- Flip (horizontal, vertical)
- Grid View (multiple image comparison)

---

### 2.11 PACS Integration

#### SRS-FR-034: DICOM C-ECHO
**Traces to**: PRD FR-010.1

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-010 |
| Dependency | pacs_system/network |

**Description**: The system shall test connectivity with PACS server.

**Specification**:
- C-ECHO SCU functionality implementation
- Verification SOP Class (1.2.840.10008.1.1)

---

#### SRS-FR-035: DICOM C-FIND
**Traces to**: PRD FR-010.2

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-010 |
| Dependency | pacs_system/services |

**Description**: The system shall search for patient/study/series/images on PACS server.

**Query Levels**:

| Level | SOP Class UID |
|-------|---------------|
| Patient | 1.2.840.10008.5.1.4.1.2.1.1 |
| Study | 1.2.840.10008.5.1.4.1.2.2.1 |
| Series | Included in Study |
| Image | Included in Series |

**Search Fields**:
- Patient Name (partial match with *)
- Patient ID
- Study Date (range)
- Modality
- Accession Number

---

#### SRS-FR-036: DICOM C-MOVE
**Traces to**: PRD FR-010.3

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-010 |
| Dependency | pacs_system/services |

**Description**: The system shall retrieve images from PACS server.

**Specification**:
- C-MOVE SCU functionality
- Progress display (Pending status handling)
- Requires receive SCP (SRS-FR-037)

---

#### SRS-FR-037: DICOM C-STORE SCP
**Traces to**: PRD FR-010.4

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-010 |
| Dependency | pacs_system/services |

**Description**: The system shall receive DICOM images transmitted from external sources.

**Specification** (Ref: REF-005):
- Storage SCP implementation
- Supported SOP Classes: CT, MRI, Secondary Capture, etc.
- Receive callback: Auto-load or notification on image receive

---

#### SRS-FR-038: PACS Server Configuration
**Traces to**: PRD FR-010.5

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-010 |
| Dependency | Qt Settings |

**Description**: The system shall manage PACS server connection information.

**Configuration Parameters**:
- AE Title (Calling)
- AE Title (Called)
- Host (IP or hostname)
- Port (default: 104)
- Description (optional)

**Storage**: Qt QSettings (INI or Registry)

---

### 2.12 User Interface

#### SRS-FR-039: Main Window Layout
**Traces to**: PRD FR-011.1 ~ FR-011.6

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-011 |
| Dependency | Qt Widgets |

**Description**: The system shall configure the main window layout.

**Components**:
- **Menu Bar**: File, Edit, View, Tools, Image, Window, Help
- **Tool Bar**: Frequently used tool buttons
- **Patient Browser**: Patient/Study/Series tree view (left)
- **Main Viewport**: 2D/3D image view (center)
- **Properties Panel**: Tool options, settings (right)
- **Status Bar**: Status information (bottom)

**Features**:
- Qt Docking Widget based panel structure
- Layout save/restore (`QSettings`)
- Multi-monitor support (window separation)
- Dark theme default support (Fusion style + custom palette)

---

#### SRS-FR-040: Keyboard Shortcuts
**Traces to**: PRD FR-011.4

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-011 |
| Dependency | Qt Shortcuts |

**Default Shortcuts**:

| Action | Shortcut |
|--------|----------|
| Open File | Ctrl+O |
| Save | Ctrl+S |
| Undo | Ctrl+Z |
| Redo | Ctrl+Y |
| Zoom In | + |
| Zoom Out | - |
| Reset View | R |
| Next Slice | PgDn / Mouse Wheel Down |
| Previous Slice | PgUp / Mouse Wheel Up |
| Toggle Crosshair | C |
| Toggle Annotations | A |

---

## 3. Non-Functional Requirements

### 3.1 Performance Requirements

#### SRS-NFR-001: DICOM Series Loading Time
**Traces to**: PRD NFR-001

| Attribute | Value |
|-----------|-------|
| Metric | Time to load and display |
| Target | ≤ 3 seconds |
| Condition | 512×512×300 CT volume |
| Measurement | From menu click to first render |

**Implementation Considerations**:
- Asynchronous loading (QThread or std::async)
- Progress bar display
- Streaming loading (when possible)

---

#### SRS-NFR-002: Volume Rendering Frame Rate
**Traces to**: PRD NFR-002

| Attribute | Value |
|-----------|-------|
| Metric | Frames per second |
| Target | ≥ 30 FPS |
| Condition | Interactive rotation/zoom |
| Hardware | OpenGL 4.1+ GPU |

**Implementation Considerations**:
- GPU Ray Casting (`vtkGPUVolumeRayCastMapper`)
- LOD (Level of Detail) during interaction
- Reduced sampling distance during rotation

---

#### SRS-NFR-003: MPR Slice Navigation Response
**Traces to**: PRD NFR-003

| Attribute | Value |
|-----------|-------|
| Metric | Latency |
| Target | ≤ 100ms |
| Condition | Slice scroll with mouse wheel |

---

#### SRS-NFR-004: Memory Usage
**Traces to**: PRD NFR-004

| Attribute | Value |
|-----------|-------|
| Metric | Peak memory usage |
| Target | ≤ 2GB |
| Condition | 1GB volume data loaded |

**Implementation Considerations**:
- Maintain single data copy (ITK→VTK direct connection)
- Release unused data
- Memory mapping (large files)

---

#### SRS-NFR-005: Application Startup Time
**Traces to**: PRD NFR-005

| Attribute | Value |
|-----------|-------|
| Metric | Cold start time |
| Target | ≤ 5 seconds |
| Condition | No files opened |

---

### 3.2 Compatibility Requirements

#### SRS-NFR-006: Operating System Support
**Traces to**: PRD NFR-006

| OS | Minimum Version |
|----|-----------------|
| macOS | 12 (Monterey) |
| Windows | 10 (64-bit) |
| Ubuntu | 20.04 LTS |

---

#### SRS-NFR-007: Graphics Requirements
**Traces to**: PRD NFR-007

| Requirement | Specification |
|-------------|---------------|
| OpenGL | 4.1+ |
| VRAM | 2GB+ (recommended 4GB) |
| GPU | Discrete GPU recommended |

---

#### SRS-NFR-008: DICOM Standard Compliance
**Traces to**: PRD NFR-008

| Standard | Parts |
|----------|-------|
| DICOM PS3 | Part 3 (IOD), Part 5 (Encoding), Part 6 (Dictionary), Part 10 (Media), Part 18 (Web) |

---

#### SRS-NFR-009: Transfer Syntax Support
**Traces to**: PRD NFR-009

See SRS-FR-003 for supported Transfer Syntax list.

---

### 3.3 Usability Requirements

#### SRS-NFR-010: Feature Accessibility
**Traces to**: PRD NFR-010

| Requirement | Target |
|-------------|--------|
| Main features access | ≤ 3 clicks |
| Tool discovery | Toolbar + Menu consistency |

---

#### SRS-NFR-011: Learning Curve
**Traces to**: PRD NFR-011

| Requirement | Target |
|-------------|--------|
| Basic operation training | ≤ 30 minutes |
| Documentation | Context-sensitive help (F1) |

---

#### SRS-NFR-012: Error Handling
**Traces to**: PRD NFR-012

| Requirement | Implementation |
|-------------|----------------|
| Error messages | Clear, actionable messages |
| Error logging | File-based log with severity levels |
| Recovery | Graceful degradation, no crash |

---

### 3.4 Security Requirements

#### SRS-NFR-013: Patient Information Anonymization
**Traces to**: PRD NFR-013

| Tags to Anonymize |
|-------------------|
| (0010,0010) Patient's Name |
| (0010,0020) Patient ID |
| (0010,0030) Patient's Birth Date |
| (0010,0040) Patient's Sex |
| (0008,0050) Accession Number |

**Implementation**: Use pacs_system anonymization module

---

#### SRS-NFR-014: PACS Communication Encryption
**Traces to**: PRD NFR-014

| Requirement | Implementation |
|-------------|----------------|
| TLS Support | pacs_system TLS 1.2+ |
| Certificate | Configurable CA certificate |

---

#### SRS-NFR-015: Local Configuration Encryption
**Traces to**: PRD NFR-015

| Data | Encryption |
|------|------------|
| PACS Credentials | AES-256 encrypted storage |
| Settings | Plain (non-sensitive) |

---

## 4. Interface Requirements

### 4.1 Software Interfaces

#### SRS-IF-001: ITK Interface
**Traces to**: REF-001, REF-003

| Component | Interface |
|-----------|-----------|
| Image Processing | `itk::Image<TPixel, VDimension>` |
| Segmentation | `itk::ImageToImageFilter` subclasses |
| VTK Bridge | `itk::ImageToVTKImageFilter`, `itk::VTKImageToImageFilter` |

---

#### SRS-IF-002: VTK Interface
**Traces to**: REF-002, REF-003

| Component | Interface |
|-----------|-----------|
| Rendering | `vtkRenderer`, `vtkRenderWindow` |
| Volume | `vtkGPUVolumeRayCastMapper`, `vtkVolume` |
| Surface | `vtkPolyDataMapper`, `vtkActor` |
| Qt Integration | `QVTKOpenGLNativeWidget` |

---

#### SRS-IF-003: pacs_system Interface
**Traces to**: REF-005

| Component | Interface |
|-----------|-----------|
| File I/O | `pacs::dicom_file`, `pacs::dicom_dataset` |
| Network | `pacs::association`, `pacs::query_scu`, `pacs::retrieve_scu` |
| Encoding | `pacs::codec_factory`, `pacs::transfer_syntax` |

---

### 4.2 User Interfaces

#### SRS-IF-004: Main Window UI
**Traces to**: PRD FR-011

See SRS-FR-039 for detailed layout specification.

---

### 4.3 Hardware Interfaces

#### SRS-IF-005: Graphics Hardware

| Requirement | Specification |
|-------------|---------------|
| OpenGL | 4.1 Core Profile |
| Framebuffer | Off-screen rendering support |
| Multi-monitor | Extended desktop support |

---

## 5. Data Requirements

### 5.1 Input Data

#### SRS-DR-001: DICOM Data
**Traces to**: PRD Section 9 Appendix A

| Modality | SOP Class | Support |
|----------|-----------|---------|
| CT | CT Image Storage | Required |
| MRI | MR Image Storage | Required |
| DR | Digital X-Ray Image Storage | Required |
| CR | Computed Radiography Image Storage | Required |
| Secondary Capture | Secondary Capture Image Storage | Required |

---

### 5.2 Output Data

#### SRS-DR-002: Export Formats

| Data Type | Formats |
|-----------|---------|
| Segmentation Mask | NRRD, NIfTI, ITK MetaImage |
| Surface Mesh | STL, PLY, OBJ |
| Measurement Data | JSON, CSV |
| Report | PDF, DICOM SR |
| Screenshot | PNG, JPEG |

---

## 6. Constraints

### 6.1 Development Constraints

#### SRS-CR-001: Technology Stack

| Component | Technology |
|-----------|------------|
| Language | C++20 |
| Build System | CMake 3.20+ |
| GUI Framework | Qt 6.5+ |
| Image Processing | ITK 5.4+ |
| Visualization | VTK 9.3+ |
| DICOM Processing | pacs_system (kcenon ecosystem) |

---

#### SRS-CR-002: Coding Standards

| Standard | Application |
|----------|-------------|
| C++ Core Guidelines | General C++ |
| Qt Coding Style | Qt-specific code |
| pacs_system conventions | DICOM handling |

---

### 6.2 Regulatory Constraints

#### SRS-CR-003: Medical Device Considerations

> **Note**: The current version (v0.x.x) is for research/development purposes and is not subject to medical device certification. If medical device certification is required in the future, IEC 62304 (Medical Device Software) compliance will be necessary.

---

## 7. Traceability Matrix

### 7.1 PRD to SRS Traceability

| PRD ID | PRD Description | SRS ID(s) |
|--------|-----------------|-----------|
| FR-001.1 | DICOM series load | SRS-FR-001, SRS-FR-002 |
| FR-001.2 | Automatic slice sorting | SRS-FR-002 |
| FR-001.3 | Compression format decoding | SRS-FR-003 |
| FR-001.4 | Metadata extraction | SRS-FR-001 |
| FR-001.5 | HU value conversion | SRS-FR-004 |
| FR-002.1 | GPU Ray Casting | SRS-FR-005 |
| FR-002.2 | Color Transfer Function | SRS-FR-006 |
| FR-002.3 | Opacity Transfer Function | SRS-FR-006 |
| FR-002.4 | Presets | SRS-FR-006 |
| FR-002.5 | Lighting/Shading | SRS-FR-005 |
| FR-002.6 | Volume Clipping | SRS-FR-007 |
| FR-003.1 | Axial/Coronal/Sagittal | SRS-FR-008 |
| FR-003.2 | Slice scroll | SRS-FR-008 |
| FR-003.3 | Crosshair synchronization | SRS-FR-008 |
| FR-003.4 | Window/Level | SRS-FR-009 |
| FR-003.5 | Oblique reslice | SRS-FR-010 |
| FR-003.6 | Thick Slab | SRS-FR-011 |
| FR-004.1 | Marching Cubes | SRS-FR-012 |
| FR-004.2 | Threshold surface generation | SRS-FR-012 |
| FR-004.3 | Smoothing/Decimation | SRS-FR-013 |
| FR-004.4 | Multi-surface rendering | SRS-FR-014 |
| FR-004.5 | STL/PLY export | SRS-FR-015 |
| FR-005.1 | Gaussian Smoothing | SRS-FR-016 |
| FR-005.2 | Anisotropic Diffusion | SRS-FR-017 |
| FR-005.4 | N4 Bias Correction | SRS-FR-018 |
| FR-005.5 | Isotropic Resampling | SRS-FR-019 |
| FR-006.1 | Manual threshold segmentation | SRS-FR-020 |
| FR-006.2 | Otsu automatic threshold | SRS-FR-020 |
| FR-006.3 | Region Growing | SRS-FR-021 |
| FR-006.4 | Confidence Connected | SRS-FR-021 |
| FR-006.5 | Level Set | SRS-FR-022 |
| FR-006.7~12 | Manual segmentation tools | SRS-FR-023 |
| FR-006.13~18 | Multi-label | SRS-FR-024 |
| FR-006.19~25 | Morphological post-processing | SRS-FR-025 |
| FR-007.1~5 | Linear measurement | SRS-FR-026 |
| FR-007.6~10 | Area measurement | SRS-FR-027 |
| FR-007.15~20 | ROI statistics | SRS-FR-028 |
| FR-007.11~14 | Volume measurement | SRS-FR-029 |
| FR-007.21~25 | Advanced quantitative analysis | SRS-FR-030 |
| FR-012.1~8 | ROI management | SRS-FR-031 |
| FR-013.1~6 | Analysis report | SRS-FR-032 |
| FR-008.1~6 | 2D image viewing | SRS-FR-033 |
| FR-010.1 | C-ECHO | SRS-FR-034 |
| FR-010.2 | C-FIND | SRS-FR-035 |
| FR-010.3 | C-MOVE | SRS-FR-036 |
| FR-010.4 | C-STORE SCP | SRS-FR-037 |
| FR-010.5 | PACS configuration | SRS-FR-038 |
| FR-011.1~6 | UI requirements | SRS-FR-039, SRS-FR-040 |
| NFR-001 | Loading time | SRS-NFR-001 |
| NFR-002 | Rendering FPS | SRS-NFR-002 |
| NFR-003 | Slice transition response | SRS-NFR-003 |
| NFR-004 | Memory usage | SRS-NFR-004 |
| NFR-005 | Startup time | SRS-NFR-005 |
| NFR-006 | Operating systems | SRS-NFR-006 |
| NFR-007 | Graphics card | SRS-NFR-007 |
| NFR-008 | DICOM standard | SRS-NFR-008 |
| NFR-009 | Transfer Syntax | SRS-NFR-009, SRS-FR-003 |
| NFR-010 | 3-click access | SRS-NFR-010 |
| NFR-011 | Learning time | SRS-NFR-011 |
| NFR-012 | Error messages | SRS-NFR-012 |
| NFR-013 | Anonymization | SRS-NFR-013 |
| NFR-014 | TLS encryption | SRS-NFR-014 |
| NFR-015 | Settings encryption | SRS-NFR-015 |

### 7.2 SRS to Reference Document Traceability

| SRS ID | Reference Document |
|--------|-------------------|
| SRS-FR-001~004 | REF-004, REF-005 |
| SRS-FR-005~007 | REF-002 |
| SRS-FR-008~011 | REF-002 |
| SRS-FR-012~015 | REF-002 |
| SRS-FR-016~019 | REF-001, REF-004 |
| SRS-FR-020~025 | REF-001, REF-004 |
| SRS-FR-026~030 | REF-002, REF-004 |
| SRS-FR-034~037 | REF-005 |

---

## 8. Appendix

### A. Glossary

| Term | Definition |
|------|------------|
| Axial | XY plane (Top-down view) |
| Coronal | XZ plane (Front view) |
| Sagittal | YZ plane (Side view) |
| Isosurface | 3D surface of constant value |
| Marching Cubes | Isosurface extraction algorithm |
| Ray Casting | Volume rendering technique |
| Transfer Function | Function mapping scalar values to color/opacity |
| Voxel | 3D pixel (Volume Element) |

### B. File Formats Summary

| Format | Extension | Read | Write | Use |
|--------|-----------|------|-------|-----|
| DICOM | .dcm | ✅ | ✅ | Medical images |
| NRRD | .nrrd | ✅ | ✅ | Segmentation masks |
| NIfTI | .nii, .nii.gz | ✅ | ✅ | Segmentation masks |
| STL | .stl | ❌ | ✅ | 3D mesh |
| PLY | .ply | ❌ | ✅ | 3D mesh (with color) |
| PNG | .png | ❌ | ✅ | Screenshots |
| PDF | .pdf | ❌ | ✅ | Reports |

---

## Version History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1.0 | 2025-12-31 | Development Team | Initial SRS |
| 0.2.0 | 2025-12-31 | Development Team | Added segmentation (SRS-FR-020~025), measurement (SRS-FR-026~030), ROI management (SRS-FR-031) |
| 0.3.0 | 2026-02-11 | Development Team | Replaced DCMTK with pacs_system for DICOM network operations; version sync with build system |

---

*This document is subject to change based on design decisions and technical discoveries during development.*

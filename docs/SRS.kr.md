# DICOM Viewer - Software Requirements Specification (SRS)

> **Version**: 0.2.0
> **Created**: 2025-12-31
> **Last Updated**: 2025-12-31
> **Status**: Draft (Pre-release)
> **Author**: Development Team
> **Based on**: [PRD v0.2.0](PRD.md)

---

## Document Information

### Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 0.1.0 | 2025-12-31 | Development Team | Initial SRS based on PRD 0.1.0 |
| 0.2.0 | 2025-12-31 | Development Team | Added segmentation and measurement requirements (FR-006, FR-007 expansion) |

### Referenced Documents

| Document ID | Title | Location |
|-------------|-------|----------|
| PRD-001 | Product Requirements Document | [PRD.md](PRD.md) |
| REF-001 | ITK Overview | [reference/01-itk-overview.md](reference/01-itk-overview.md) |
| REF-002 | VTK Overview | [reference/02-vtk-overview.md](reference/02-vtk-overview.md) |
| REF-003 | ITK-VTK Integration | [reference/03-itk-vtk-integration.md](reference/03-itk-vtk-integration.md) |
| REF-004 | DICOM Pipeline | [reference/04-dicom-pipeline.md](reference/04-dicom-pipeline.md) |
| REF-005 | PACS Integration | [reference/05-pacs-integration.md](reference/05-pacs-integration.md) |

### Requirement ID Convention

- **SRS-FR-XXX**: Functional Requirements (기능 요구사항)
- **SRS-NFR-XXX**: Non-Functional Requirements (비기능 요구사항)
- **SRS-IF-XXX**: Interface Requirements (인터페이스 요구사항)
- **SRS-DR-XXX**: Data Requirements (데이터 요구사항)
- **SRS-CR-XXX**: Constraint Requirements (제약사항)

---

## 1. Introduction

### 1.1 Purpose

본 문서는 DICOM Viewer 소프트웨어의 상세 소프트웨어 요구사항 명세서(SRS)입니다. PRD에서 정의된 제품 요구사항을 기술적 소프트웨어 요구사항으로 변환하고, 구현에 필요한 상세 명세를 제공합니다.

### 1.2 Scope

DICOM Viewer는 의료 영상(CT, MRI, DR/CR) 뷰어로서:
- CT/MRI 3D 볼륨 렌더링 및 MPR 뷰
- 영역 세그먼테이션 및 측정
- DR/CR 2D 뷰잉
- PACS 연동

을 제공하는 데스크탑 애플리케이션입니다.

### 1.3 Definitions, Acronyms, and Abbreviations

| 용어 | 정의 |
|------|------|
| **DICOM** | Digital Imaging and Communications in Medicine - 의료 영상 표준 |
| **HU** | Hounsfield Unit - CT 영상의 밀도 측정 단위 |
| **MPR** | Multi-Planar Reconstruction - 다평면 재구성 |
| **PACS** | Picture Archiving and Communication System |
| **ROI** | Region of Interest - 관심 영역 |
| **VR** | Volume Rendering - 볼륨 렌더링 |
| **LPS** | Left-Posterior-Superior 좌표계 |
| **Transfer Syntax** | DICOM 데이터 인코딩 방식 |
| **SCP** | Service Class Provider - DICOM 서비스 제공자 |
| **SCU** | Service Class User - DICOM 서비스 사용자 |

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

**Description**: 시스템은 로컬 파일 시스템에서 DICOM 파일을 읽고 파싱할 수 있어야 한다.

**Specification**:
- pacs_system의 `dicom_file::open()` API를 사용하여 DICOM 파일 파싱
- 지원 DICOM Part 10 형식: Preamble(128 bytes) + "DICM" Prefix + Meta Information + Dataset
- 필수 메타데이터 추출:
  - Patient Module: (0010,0010) Patient Name, (0010,0020) Patient ID
  - Study Module: (0020,000D) Study Instance UID, (0008,0020) Study Date
  - Series Module: (0020,000E) Series Instance UID, (0008,0060) Modality
  - Image Module: (0028,0010) Rows, (0028,0011) Columns, (0028,0100) Bits Allocated

**Input**:
- 파일 경로 (std::filesystem::path)

**Output**:
- pacs::dicom_file 객체 또는 에러 코드

**Verification**:
- 유효한 DICOM 파일 로드 시 성공 반환
- 유효하지 않은 파일 로드 시 명확한 에러 메시지

---

#### SRS-FR-002: DICOM Series Assembly
**Traces to**: PRD FR-001.2

| Attribute | Value |
|-----------|-------|
| Priority | P0 (Critical) |
| Source | PRD FR-001 |
| Dependency | SRS-FR-001 |

**Description**: 시스템은 디렉토리에서 DICOM 시리즈를 자동으로 감지하고 3D 볼륨으로 조합할 수 있어야 한다.

**Specification**:
- 디렉토리 스캔하여 모든 DICOM 파일 탐지
- Series Instance UID (0020,000E)로 시리즈 그룹화
- 슬라이스 정렬 알고리즘:
  1. **Primary**: Image Position Patient (0020,0032)의 Z 좌표 기반 정렬
  2. **Fallback 1**: Slice Location (0020,1041) 기반 정렬
  3. **Fallback 2**: Instance Number (0020,0013) 기반 정렬
- 정렬된 슬라이스를 ITK Image로 변환

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
- 디렉토리 경로

**Output**:
- `itk::Image<short, 3>` (CT) 또는 `itk::Image<unsigned short, 3>` (MRI)

---

#### SRS-FR-003: Transfer Syntax Decoding
**Traces to**: PRD FR-001.3

| Attribute | Value |
|-----------|-------|
| Priority | P0 (Critical) |
| Source | PRD FR-001 |
| Dependency | pacs_system/encoding |

**Description**: 시스템은 다양한 DICOM Transfer Syntax의 압축 데이터를 자동으로 디코딩할 수 있어야 한다.

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
- pacs_system의 `codec_factory::create(transferSyntaxUID)` 사용
- Transfer Syntax UID (0002,0010)에서 압축 방식 결정
- 자동 코덱 선택 및 디코딩

---

#### SRS-FR-004: Hounsfield Unit Conversion
**Traces to**: PRD FR-001.5

| Attribute | Value |
|-----------|-------|
| Priority | P0 (Critical) |
| Source | PRD FR-001 |
| Dependency | SRS-FR-001 |

**Description**: CT 영상의 저장된 픽셀 값을 Hounsfield Unit으로 변환해야 한다.

**Specification**:
```
HU = StoredValue × RescaleSlope + RescaleIntercept
```

- **Rescale Slope**: (0028,1053), 기본값 1.0
- **Rescale Intercept**: (0028,1052), 기본값 0.0

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

**Description**: 시스템은 GPU 가속 Ray Casting을 사용하여 볼륨 렌더링을 수행해야 한다.

**Specification**:
- **Primary Mapper**: `vtkGPUVolumeRayCastMapper`
- **Fallback Mapper**: `vtkSmartVolumeMapper` (GPU 미지원 시 CPU로 폴백)
- Blend Mode: Composite (기본), Maximum Intensity Projection (MIP)
- Sampling Distance: Automatic (based on data spacing)

**Performance Requirements**:
- 512×512×300 볼륨: 30 FPS 이상 (Ref: PRD NFR-002)
- 인터랙티브 렌더링: LOD (Level of Detail) 적용

**Implementation** (Ref: REF-002):
```cpp
auto volumeMapper = vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
volumeMapper->SetInputConnection(imageData->GetOutputPort());
volumeMapper->SetBlendModeToComposite();

// GPU 지원 확인 및 폴백
if (!volumeMapper->IsRenderSupported(renderWindow, volumeProperty)) {
    auto smartMapper = vtkSmartPointer<vtkSmartVolumeMapper>::New();
    // ... CPU 폴백 설정
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

**Description**: 볼륨 렌더링의 Color/Opacity Transfer Function을 관리할 수 있어야 한다.

**Specification**:

**Color Transfer Function** (`vtkColorTransferFunction`):
- RGB 색상 매핑 포인트 추가/수정/삭제
- 선형 보간 (기본) 또는 단계별 보간

**Opacity Transfer Function** (`vtkPiecewiseFunction`):
- 스칼라 값 → 불투명도 매핑
- Gradient Opacity 지원 (경계면 강조)

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

**Description**: 볼륨의 일부를 클리핑(잘라내기)하여 내부 구조를 볼 수 있어야 한다.

**Specification**:
- `vtkBoxWidget2` 기반 인터랙티브 클리핑 박스
- 6면 클리핑 평면 개별 조절 가능
- 클리핑 영역 내/외부 토글 가능

---

### 2.3 Multi-Planar Reconstruction (MPR)

#### SRS-FR-008: Orthogonal MPR Views
**Traces to**: PRD FR-003.1, FR-003.2, FR-003.3

| Attribute | Value |
|-----------|-------|
| Priority | P0 (Critical) |
| Source | PRD FR-003 |
| Dependency | VTK ImagingCore |

**Description**: Axial, Coronal, Sagittal 세 개의 직교 평면 뷰를 동시에 표시해야 한다.

**Specification**:

**Reslice Matrix Definitions** (Ref: REF-002):

| View | Orientation | vtkImageReslice Matrix |
|------|-------------|------------------------|
| Axial (XY) | Top-down | Identity + Z translation |
| Coronal (XZ) | Front | Rotate -90° around X axis |
| Sagittal (YZ) | Side | Rotate 90° around Y axis |

**Features**:
- 마우스 휠: 슬라이스 스크롤
- 마우스 드래그 (Left): Window/Level 조절
- 마우스 드래그 (Right): Pan
- 마우스 드래그 (Middle): Zoom

**Crosshair Synchronization**:
- 한 뷰에서 클릭 시 다른 뷰의 크로스헤어 업데이트
- 물리적 좌표 기반 동기화 (LPS 좌표계)

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

**Description**: 마우스 드래그로 Window/Level 값을 실시간 조절할 수 있어야 한다.

**Specification**:
- **Window Width (W)**: 표시되는 HU 범위의 폭
- **Window Level/Center (L)**: 표시되는 HU 범위의 중심값
- **표시 범위**: [L - W/2, L + W/2]

**Mouse Interaction**:
- X 방향 드래그: Window Width 조절
- Y 방향 드래그: Window Level 조절
- 감도: 1 pixel = 1 HU 단위

**Presets** (2D Viewing):

| Preset | Window Width | Window Center | Use |
|--------|--------------|---------------|-----|
| Chest | 1500 | -600 | 흉부 X-ray |
| Abdomen | 400 | 40 | 복부 |
| Bone | 2000 | 300 | 골격 |
| Brain | 80 | 40 | 뇌 CT |
| Liver | 150 | 30 | 간 |
| Mediastinum | 350 | 50 | 종격동 |

---

#### SRS-FR-010: Oblique Reslicing
**Traces to**: PRD FR-003.5

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-003 |
| Dependency | SRS-FR-008 |

**Description**: 임의의 각도로 기울어진 평면에서 슬라이스를 추출할 수 있어야 한다.

**Specification**:
- `vtkImagePlaneWidget` 또는 `vtkImageReslice` 사용
- 회전 각도: -180° ~ +180° (모든 축)
- 평면 정의: 중심점 + 법선 벡터
- 실시간 인터랙티브 조작

---

#### SRS-FR-011: Thick Slab Reconstruction
**Traces to**: PRD FR-003.6

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-003 |
| Dependency | SRS-FR-008 |

**Description**: 여러 슬라이스를 조합하여 Thick Slab 영상을 생성할 수 있어야 한다.

**Specification**:

| Mode | Algorithm | Use Case |
|------|-----------|----------|
| MIP | Maximum Intensity Projection | 혈관 조영 |
| MinIP | Minimum Intensity Projection | 기도 시각화 |
| AverageIP | Average Intensity Projection | 노이즈 감소 |

**Parameters**:
- Slab Thickness: 1mm ~ 50mm (사용자 설정)
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

**Description**: Marching Cubes 알고리즘을 사용하여 등표면을 추출해야 한다.

**Specification** (Ref: REF-002):
- **Algorithm**: `vtkMarchingCubes` 또는 `vtkContourFilter`
- **Input**: `vtkImageData` (3D 볼륨)
- **Output**: `vtkPolyData` (삼각형 메시)
- **Threshold**: 사용자 지정 isovaluefor (CT용 HU 기반)

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

**Description**: 추출된 표면을 스무딩하고 폴리곤 수를 줄여 렌더링 성능을 향상시킬 수 있어야 한다.

**Specification**:

**Smoothing**:
- `vtkWindowedSincPolyDataFilter` (권장) 또는 `vtkSmoothPolyDataFilter`
- Iterations: 10 ~ 50 (사용자 설정)
- Pass Band: 0.05 ~ 0.2

**Decimation**:
- `vtkDecimatePro` 또는 `vtkQuadricDecimation`
- Target Reduction: 0.3 ~ 0.8 (30% ~ 80% 감소)
- Preserve Topology: On

---

#### SRS-FR-014: Multi-Surface Rendering
**Traces to**: PRD FR-004.4

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-004 |
| Dependency | SRS-FR-012 |

**Description**: 여러 조직의 표면을 서로 다른 색상으로 동시에 렌더링할 수 있어야 한다.

**Specification**:
- 각 표면에 대해 별도의 `vtkActor` 생성
- 색상, 투명도, 스펙큘러 속성 개별 설정
- 표면별 표시/숨김 토글

---

#### SRS-FR-015: Mesh Export
**Traces to**: PRD FR-004.5

| Attribute | Value |
|-----------|-------|
| Priority | P2 (Medium) |
| Source | PRD FR-004 |
| Dependency | SRS-FR-012 |

**Description**: 추출된 표면 메시를 외부 포맷으로 내보낼 수 있어야 한다.

**Supported Formats**:

| Format | Extension | Writer Class | Use Case |
|--------|-----------|--------------|----------|
| STL (Binary) | .stl | `vtkSTLWriter` | 3D 프린팅 |
| STL (ASCII) | .stl | `vtkSTLWriter` | 호환성 |
| PLY | .ply | `vtkPLYWriter` | 색상 포함 |
| OBJ | .obj | `vtkOBJWriter` | 범용 |

---

### 2.5 Image Preprocessing

#### SRS-FR-016: Gaussian Smoothing
**Traces to**: PRD FR-005.1

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-005 |
| Dependency | ITK Smoothing |

**Description**: Gaussian 필터를 적용하여 노이즈를 제거할 수 있어야 한다.

**Specification** (Ref: REF-001):
- **Filter**: `itk::DiscreteGaussianImageFilter`
- **Parameter**: Variance (σ²), 기본값 1.0
- **Kernel Width**: Automatic (기본) 또는 사용자 지정 (최대 32)

**Input**: `itk::Image<short, 3>`
**Output**: `itk::Image<short, 3>` (동일 타입)

---

#### SRS-FR-017: Anisotropic Diffusion
**Traces to**: PRD FR-005.2

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-005 |
| Dependency | ITK Smoothing |

**Description**: 엣지를 보존하면서 노이즈를 제거하는 비등방성 확산 필터를 적용할 수 있어야 한다.

**Specification** (Ref: REF-001):
- **Filter**: `itk::CurvatureAnisotropicDiffusionImageFilter`
- **Parameters**:
  - Time Step: 0.0625 (3D 안정성 조건)
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

**Description**: MRI 영상의 불균일한 강도 분포를 보정할 수 있어야 한다.

**Specification** (Ref: REF-004):
- **Filter**: `itk::N4BiasFieldCorrectionImageFilter`
- **Parameters**:
  - Fitting Levels: 4 (기본)
  - Maximum Iterations: [50, 50, 50, 50]
  - Convergence Threshold: 0.001

**Input**: `itk::Image<float, 3>` + 마스크(선택)
**Output**: 보정된 `itk::Image<float, 3>`

---

#### SRS-FR-019: Isotropic Resampling
**Traces to**: PRD FR-005.5

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-005 |
| Dependency | ITK ImageFunction |

**Description**: 비등방성 복셀을 등방성 복셀로 리샘플링할 수 있어야 한다.

**Specification** (Ref: REF-004):
- **Filter**: `itk::ResampleImageFilter`
- **Interpolator**: `itk::LinearInterpolateImageFunction` (기본)
- **Target Spacing**: 사용자 지정 (예: 1.0mm 등방성)

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

**Description**: HU 값 범위를 기반으로 영역을 분할할 수 있어야 한다.

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

**Description**: 시드 포인트 기반 영역 성장 알고리즘으로 분할할 수 있어야 한다.

**Specification** (Ref: REF-001):

**Connected Threshold**:
- **Filter**: `itk::ConnectedThresholdImageFilter`
- **Input**: Seed point (3D index), Lower/Upper threshold
- **Output**: Binary mask

**Confidence Connected**:
- **Filter**: `itk::ConfidenceConnectedImageFilter`
- **Parameters**:
  - Multiplier: 표준편차 배수 (기본 2.5)
  - Iterations: 5 (기본)
  - Initial Neighborhood Radius: 2 (기본)

**User Interaction**:
- MPR 뷰에서 클릭하여 시드 포인트 설정
- 물리적 좌표 → 인덱스 좌표 변환 필요

---

#### SRS-FR-022: Level Set Segmentation
**Traces to**: PRD FR-006.5

| Attribute | Value |
|-----------|-------|
| Priority | P2 (Medium) |
| Source | PRD FR-006 |
| Dependency | ITK LevelSets |

**Description**: Level Set 기반 분할 알고리즘을 적용할 수 있어야 한다.

**Specification** (Ref: REF-001):
- **Pipeline**:
  1. Gradient Magnitude (`itk::GradientMagnitudeRecursiveGaussianImageFilter`)
  2. Sigmoid Feature (`itk::SigmoidImageFilter`)
  3. Fast Marching (Initial Level Set) (`itk::FastMarchingImageFilter`)
  4. Geodesic Active Contour (`itk::GeodesicActiveContourLevelSetImageFilter`)
  5. Binary Thresholding

**Parameters**:
- Propagation Scaling: 1.0 (기본)
- Curvature Scaling: 1.0 (기본)
- Advection Scaling: 1.0 (기본)
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

**Description**: 수동 분할을 위한 그리기 도구를 제공해야 한다.

**Tools**:

| Tool | Description | Interaction |
|------|-------------|-------------|
| **Brush** | 원형/사각형 브러시로 그리기 | 클릭 & 드래그 |
| **Eraser** | 분할 영역 삭제 | 클릭 & 드래그 |
| **Fill** | 폐쇄 영역 채우기 | 클릭 |
| **Freehand** | 자유 곡선 그리기 | 클릭 & 드래그 |
| **Polygon** | 다각형 ROI | 클릭 (정점 추가) + 더블클릭 (완료) |
| **Smart Scissors** | 경계 추적 (LiveWire) | 클릭 (앵커) + 이동 (추적) |

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

**Description**: 여러 장기/조직을 동시에 분할하고 관리할 수 있어야 한다.

**Specification**:

**Label Management**:
- Label ID: 1 ~ 255 (0은 배경)
- Label 속성: 이름, 색상 (RGBA), 표시 여부
- 활성 Label 선택 (현재 편집 대상)

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
- NRRD (.nrrd) - 권장
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

**Description**: 분할 결과에 형태학적 연산을 적용할 수 있어야 한다.

**Operations** (Ref: REF-004):

| Operation | Filter | Effect |
|-----------|--------|--------|
| **Opening** | `itk::BinaryMorphologicalOpeningImageFilter` | 작은 돌기 제거 |
| **Closing** | `itk::BinaryMorphologicalClosingImageFilter` | 작은 구멍 채우기 |
| **Dilation** | `itk::BinaryDilateImageFilter` | 영역 확장 |
| **Erosion** | `itk::BinaryErodeImageFilter` | 영역 축소 |
| **Fill Holes** | `itk::BinaryFillholeImageFilter` | 내부 구멍 채우기 |
| **Island Removal** | `itk::BinaryShapeKeepNObjectsImageFilter` | 작은 연결 요소 제거 |

**Structuring Element**:
- Type: Ball (sphere in 3D)
- Radius: 1 ~ 10 voxels (사용자 설정)

---

### 2.7 Measurement

#### SRS-FR-026: Linear Measurement
**Traces to**: PRD FR-007.1 ~ FR-007.5

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-007 |
| Dependency | VTK Widgets |

**Description**: 두 점 사이의 거리를 측정할 수 있어야 한다.

**Specification**:

**Distance Measurement**:
- Widget: `vtkDistanceWidget` 또는 `vtkLineWidget2`
- Unit: mm (DICOM Pixel Spacing 기반)
- Precision: 소수점 2자리

**Angle Measurement**:
- Widget: `vtkAngleWidget`
- Unit: degrees (°)
- 3점 지정 (vertex가 각도의 정점)

**Annotation Display**:
- 측정 결과를 화면에 오버레이
- 폰트 크기, 색상 사용자 설정 가능

---

#### SRS-FR-027: Area Measurement (ROI)
**Traces to**: PRD FR-007.6 ~ FR-007.10

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-007 |
| Dependency | VTK Widgets |

**Description**: 2D ROI 내부의 면적을 측정할 수 있어야 한다.

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

**Description**: ROI 또는 분할 영역 내의 통계값을 계산할 수 있어야 한다.

**Statistics** (Ref: REF-004):
- **Mean**: 평균 HU/신호 강도
- **Standard Deviation**: 표준편차
- **Minimum/Maximum**: 최소/최대값
- **Median**: 중앙값
- **Pixel/Voxel Count**: 픽셀/복셀 개수

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
- 히스토그램 표시 (Qt Charts 또는 VTK Charts)
- Bin 수: 자동 또는 사용자 지정

---

#### SRS-FR-029: Volume Measurement
**Traces to**: PRD FR-007.11 ~ FR-007.14

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-007 |
| Dependency | SRS-FR-024 |

**Description**: 분할된 영역의 3D 볼륨을 계산할 수 있어야 한다.

**Calculation** (Ref: REF-004):
```
Volume (mm³) = Voxel Count × (Spacing[0] × Spacing[1] × Spacing[2])
Volume (cm³) = Volume (mm³) / 1000
Volume (mL) = Volume (cm³)  // 1 cm³ = 1 mL
```

**Output**:
- Volume: mm³, cm³, mL
- 다중 레이블 시: 레이블별 볼륨 비교 테이블

---

#### SRS-FR-030: Advanced Quantitative Analysis
**Traces to**: PRD FR-007.21 ~ FR-007.25

| Attribute | Value |
|-----------|-------|
| Priority | P2 (Medium) |
| Source | PRD FR-007 |
| Dependency | ITK ShapeLabelMap |

**Description**: 분할 영역의 고급 형태학적 특성을 분석할 수 있어야 한다.

**Metrics** (Ref: REF-004):

| Metric | Description | Formula |
|--------|-------------|---------|
| Surface Area | 표면적 (mm²) | Marching Cubes → mesh area |
| Sphericity | 구형도 | (π^(1/3) × (6V)^(2/3)) / A |
| Elongation | 신장비 | (주축 길이) / (단축 길이) |
| Centroid | 무게 중심 | Σ(position × intensity) / Σ(intensity) |
| Principal Axes | 주축 방향 | PCA on voxel positions |

**Implementation**:
- `itk::ShapeLabelMap` 및 `itk::BinaryImageToShapeLabelMapFilter` 사용

---

### 2.8 ROI Management

#### SRS-FR-031: ROI List Management
**Traces to**: PRD FR-012.1 ~ FR-012.8

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-012 |
| Dependency | Qt Widgets |

**Description**: 생성된 모든 ROI를 목록으로 관리할 수 있어야 한다.

**Features**:
- ROI 목록 패널 (TreeView 또는 TableView)
- ROI 이름 편집 (더블클릭)
- ROI 색상 변경 (Color picker)
- 표시/숨기기 토글 (체크박스)
- ROI 삭제 (Delete 키 또는 버튼)
- ROI 복사/붙여넣기 (다른 슬라이스로)

**ROI Serialization Formats**:

| Format | Extension | Use Case |
|--------|-----------|----------|
| JSON | .json | 범용, 편집 가능 |
| CSV | .csv | 스프레드시트 호환 |
| DICOM SR | .dcm | PACS 저장 |

---

### 2.9 Analysis Report

#### SRS-FR-032: Report Generation
**Traces to**: PRD FR-013.1 ~ FR-013.6

| Attribute | Value |
|-----------|-------|
| Priority | P2 (Medium) |
| Source | PRD FR-013 |
| Dependency | Qt Print Support |

**Description**: 분석 결과를 문서화된 리포트로 생성할 수 있어야 한다.

**Report Contents**:
1. **Header**: 환자 정보, 검사 날짜, 모달리티
2. **Segmentation Summary**: 레이블별 볼륨, 평균값, 표면적
3. **Measurement Results**: ROI 측정값 목록
4. **Screenshots**: 주요 뷰 스크린샷

**Export Formats**:

| Format | Library | Features |
|--------|---------|----------|
| PDF | Qt PDF Writer / libharu | 인쇄 최적화 |
| CSV/Excel | Qt CSV / libxlsx | 데이터 분석 |
| DICOM SR | pacs_system | PACS 저장 |

---

### 2.10 DR/CR 2D Viewing

#### SRS-FR-033: 2D Image Display
**Traces to**: PRD FR-008.1 ~ FR-008.6

| Attribute | Value |
|-----------|-------|
| Priority | P2 (Medium) |
| Source | PRD FR-008 |
| Dependency | VTK ImagingCore |

**Description**: 단일 프레임 DICOM 영상(DR, CR)을 2D로 표시할 수 있어야 한다.

**Features**:
- `vtkImageViewer2` 기반 2D 뷰어
- Window/Level 조절 (마우스 드래그)
- Zoom/Pan (마우스 휠, 드래그)
- Rotation (90° 단위, 자유 회전)
- Flip (좌우, 상하)
- Grid View (다중 영상 비교)

---

### 2.11 PACS Integration

#### SRS-FR-034: DICOM C-ECHO
**Traces to**: PRD FR-010.1

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-010 |
| Dependency | pacs_system/network |

**Description**: PACS 서버와의 연결을 테스트할 수 있어야 한다.

**Specification**:
- C-ECHO SCU 기능 구현
- Verification SOP Class (1.2.840.10008.1.1)

---

#### SRS-FR-035: DICOM C-FIND
**Traces to**: PRD FR-010.2

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-010 |
| Dependency | pacs_system/services |

**Description**: PACS 서버에서 환자/연구/시리즈/이미지를 검색할 수 있어야 한다.

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

**Description**: PACS 서버에서 영상을 가져올 수 있어야 한다.

**Specification**:
- C-MOVE SCU 기능
- 진행 상황 표시 (Pending 상태 처리)
- 수신 SCP (SRS-FR-037) 필요

---

#### SRS-FR-037: DICOM C-STORE SCP
**Traces to**: PRD FR-010.4

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-010 |
| Dependency | pacs_system/services |

**Description**: 외부에서 전송하는 DICOM 영상을 수신할 수 있어야 한다.

**Specification** (Ref: REF-005):
- Storage SCP 구현
- 지원 SOP Classes: CT, MRI, Secondary Capture, etc.
- 수신 콜백: 영상 수신 시 자동 로드 또는 알림

---

#### SRS-FR-038: PACS Server Configuration
**Traces to**: PRD FR-010.5

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-010 |
| Dependency | Qt Settings |

**Description**: PACS 서버 연결 정보를 관리할 수 있어야 한다.

**Configuration Parameters**:
- AE Title (Calling)
- AE Title (Called)
- Host (IP or hostname)
- Port (default: 104)
- Description (optional)

**Storage**: Qt QSettings (INI 또는 Registry)

---

### 2.12 User Interface

#### SRS-FR-039: Main Window Layout
**Traces to**: PRD FR-011.1 ~ FR-011.6

| Attribute | Value |
|-----------|-------|
| Priority | P1 (High) |
| Source | PRD FR-011 |
| Dependency | Qt Widgets |

**Description**: 메인 윈도우 레이아웃을 구성해야 한다.

**Components**:
- **Menu Bar**: File, Edit, View, Tools, Image, Window, Help
- **Tool Bar**: 자주 사용하는 도구 버튼
- **Patient Browser**: 환자/연구/시리즈 트리뷰 (왼쪽)
- **Main Viewport**: 2D/3D 영상 뷰 (중앙)
- **Properties Panel**: 도구 옵션, 설정 (오른쪽)
- **Status Bar**: 상태 정보 (하단)

**Features**:
- Qt Docking Widget 기반 패널 구조
- 레이아웃 저장/복원 (`QSettings`)
- 다중 모니터 지원 (윈도우 분리)
- 다크 테마 기본 지원 (Fusion style + custom palette)

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
- 비동기 로딩 (QThread 또는 std::async)
- 프로그레스 바 표시
- 스트리밍 로딩 (가능한 경우)

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
- 단일 데이터 복사본 유지 (ITK→VTK 직접 연결)
- 사용하지 않는 데이터 해제
- 메모리 매핑 (대용량 파일)

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
| Error messages | Clear, actionable messages in Korean |
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

**Implementation**: pacs_system의 anonymization 모듈 사용

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

> **Note**: 현재 버전(v0.x.x)은 연구/개발 목적으로, 의료기기 인증 대상이 아닙니다. 향후 의료기기 인증이 필요한 경우 IEC 62304 (Medical Device Software) 준수가 필요합니다.

---

## 7. Traceability Matrix

### 7.1 PRD to SRS Traceability

| PRD ID | PRD Description | SRS ID(s) |
|--------|-----------------|-----------|
| FR-001.1 | DICOM 시리즈 로드 | SRS-FR-001, SRS-FR-002 |
| FR-001.2 | 슬라이스 자동 정렬 | SRS-FR-002 |
| FR-001.3 | 압축 포맷 디코딩 | SRS-FR-003 |
| FR-001.4 | 메타데이터 추출 | SRS-FR-001 |
| FR-001.5 | HU 값 변환 | SRS-FR-004 |
| FR-002.1 | GPU Ray Casting | SRS-FR-005 |
| FR-002.2 | Color Transfer Function | SRS-FR-006 |
| FR-002.3 | Opacity Transfer Function | SRS-FR-006 |
| FR-002.4 | 프리셋 | SRS-FR-006 |
| FR-002.5 | 조명/쉐이딩 | SRS-FR-005 |
| FR-002.6 | 볼륨 클리핑 | SRS-FR-007 |
| FR-003.1 | Axial/Coronal/Sagittal | SRS-FR-008 |
| FR-003.2 | 슬라이스 스크롤 | SRS-FR-008 |
| FR-003.3 | 크로스헤어 동기화 | SRS-FR-008 |
| FR-003.4 | Window/Level | SRS-FR-009 |
| FR-003.5 | Oblique 리슬라이스 | SRS-FR-010 |
| FR-003.6 | Thick Slab | SRS-FR-011 |
| FR-004.1 | Marching Cubes | SRS-FR-012 |
| FR-004.2 | 임계값 표면 생성 | SRS-FR-012 |
| FR-004.3 | 스무딩/데시메이션 | SRS-FR-013 |
| FR-004.4 | 다중 표면 렌더링 | SRS-FR-014 |
| FR-004.5 | STL/PLY 내보내기 | SRS-FR-015 |
| FR-005.1 | Gaussian Smoothing | SRS-FR-016 |
| FR-005.2 | Anisotropic Diffusion | SRS-FR-017 |
| FR-005.4 | N4 Bias Correction | SRS-FR-018 |
| FR-005.5 | Isotropic Resampling | SRS-FR-019 |
| FR-006.1 | 수동 임계값 분할 | SRS-FR-020 |
| FR-006.2 | Otsu 자동 임계값 | SRS-FR-020 |
| FR-006.3 | Region Growing | SRS-FR-021 |
| FR-006.4 | Confidence Connected | SRS-FR-021 |
| FR-006.5 | Level Set | SRS-FR-022 |
| FR-006.7~12 | 수동 분할 도구 | SRS-FR-023 |
| FR-006.13~18 | 다중 레이블 | SRS-FR-024 |
| FR-006.19~25 | 형태학적 후처리 | SRS-FR-025 |
| FR-007.1~5 | 선형 측정 | SRS-FR-026 |
| FR-007.6~10 | 면적 측정 | SRS-FR-027 |
| FR-007.15~20 | ROI 통계 | SRS-FR-028 |
| FR-007.11~14 | 볼륨 측정 | SRS-FR-029 |
| FR-007.21~25 | 고급 정량 분석 | SRS-FR-030 |
| FR-012.1~8 | ROI 관리 | SRS-FR-031 |
| FR-013.1~6 | 분석 리포트 | SRS-FR-032 |
| FR-008.1~6 | 2D 영상 뷰잉 | SRS-FR-033 |
| FR-010.1 | C-ECHO | SRS-FR-034 |
| FR-010.2 | C-FIND | SRS-FR-035 |
| FR-010.3 | C-MOVE | SRS-FR-036 |
| FR-010.4 | C-STORE SCP | SRS-FR-037 |
| FR-010.5 | PACS 설정 | SRS-FR-038 |
| FR-011.1~6 | UI 요구사항 | SRS-FR-039, SRS-FR-040 |
| NFR-001 | 로딩 시간 | SRS-NFR-001 |
| NFR-002 | 렌더링 FPS | SRS-NFR-002 |
| NFR-003 | 슬라이스 전환 응답 | SRS-NFR-003 |
| NFR-004 | 메모리 사용량 | SRS-NFR-004 |
| NFR-005 | 시작 시간 | SRS-NFR-005 |
| NFR-006 | 운영체제 | SRS-NFR-006 |
| NFR-007 | 그래픽 카드 | SRS-NFR-007 |
| NFR-008 | DICOM 표준 | SRS-NFR-008 |
| NFR-009 | Transfer Syntax | SRS-NFR-009, SRS-FR-003 |
| NFR-010 | 3클릭 접근 | SRS-NFR-010 |
| NFR-011 | 학습 시간 | SRS-NFR-011 |
| NFR-012 | 에러 메시지 | SRS-NFR-012 |
| NFR-013 | 익명화 | SRS-NFR-013 |
| NFR-014 | TLS 암호화 | SRS-NFR-014 |
| NFR-015 | 설정 암호화 | SRS-NFR-015 |

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
| Axial | XY 평면 (Top-down view) |
| Coronal | XZ 평면 (Front view) |
| Sagittal | YZ 평면 (Side view) |
| Isosurface | 특정 값을 가지는 3D 등값면 |
| Marching Cubes | 등표면 추출 알고리즘 |
| Ray Casting | 볼륨 렌더링 기법 |
| Transfer Function | 스칼라 값을 색상/투명도로 매핑하는 함수 |
| Voxel | 3D 픽셀 (Volume Element) |

### B. File Formats Summary

| Format | Extension | Read | Write | Use |
|--------|-----------|------|-------|-----|
| DICOM | .dcm | ✅ | ✅ | 의료 영상 |
| NRRD | .nrrd | ✅ | ✅ | 분할 마스크 |
| NIfTI | .nii, .nii.gz | ✅ | ✅ | 분할 마스크 |
| STL | .stl | ❌ | ✅ | 3D 메시 |
| PLY | .ply | ❌ | ✅ | 3D 메시 (색상) |
| PNG | .png | ❌ | ✅ | 스크린샷 |
| PDF | .pdf | ❌ | ✅ | 리포트 |

---

## Version History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1.0 | 2025-12-31 | Development Team | Initial SRS |
| 0.2.0 | 2025-12-31 | Development Team | Added segmentation (SRS-FR-020~025), measurement (SRS-FR-026~030), ROI management (SRS-FR-031) |

---

*This document is subject to change based on design decisions and technical discoveries during development.*


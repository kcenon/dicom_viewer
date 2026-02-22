# dicom_viewer 아키텍처

> **Language:** [English](ARCHITECTURE.md) | **한국어**

## 목차

1. [개요](#개요)
2. [시스템 구조](#시스템-구조)
3. [코어 컴포넌트](#코어-컴포넌트)
4. [서비스 레이어](#서비스-레이어)
5. [UI 레이어](#ui-레이어)
6. [데이터 흐름](#데이터-흐름)
7. [설계 원칙](#설계-원칙)
8. [의존성](#의존성)
9. [성능 목표](#성능-목표)

---

## 개요

### 목적

dicom_viewer는 영상의학 전문의와 연구자를 위한 고성능 의료 영상 뷰어입니다. 3D 볼륨 렌더링, MPR(Multi-Planar Reconstruction), 세그멘테이션, 4D Flow MRI 분석, 심장 영상 워크플로우를 지원합니다.

### 설계 목표

- **성능**: GPU 가속 볼륨 렌더링으로 인터랙티브 프레임 레이트 달성
- **모듈성**: 명확한 인터페이스를 가진 독립적인 서비스 모듈
- **확장성**: 플러그인 방식의 벤더별 파서 및 내보내기 포맷
- **정확성**: DICOM 표준 준수 및 정확한 Hounsfield Unit 처리

---

## 시스템 구조

### 3계층 아키텍처

```
+-----------------------------------------------------------+
|  프레젠테이션 레이어 (Qt6)                                  |
|  MainWindow, Viewports, Panels, Dialogs                   |
+-----------------------------------------------------------+
|  서비스 레이어 (12개 모듈)                                  |
|  Render, Segmentation, Measurement, Preprocessing,        |
|  PACS, Export, Flow, Cardiac, Enhanced DICOM,             |
|  Coordinate                                               |
+-----------------------------------------------------------+
|  코어 / 데이터 레이어                                      |
|  DicomLoader, SeriesBuilder, ImageConverter,               |
|  HounsfieldConverter, ProjectManager, DataSerializer       |
+-----------------------------------------------------------+
```

### 디렉토리 구조

```
dicom_viewer/
├── include/
│   ├── core/                   # 핵심 데이터 구조 및 로더
│   │   ├── dicom_loader.hpp
│   │   ├── series_builder.hpp
│   │   ├── image_converter.hpp
│   │   ├── hounsfield_converter.hpp
│   │   ├── transfer_syntax_decoder.hpp
│   │   ├── project_manager.hpp
│   │   └── data_serializer.hpp
│   ├── services/               # 서비스 레이어 (12개 모듈)
│   │   ├── render/
│   │   ├── segmentation/
│   │   ├── measurement/
│   │   ├── preprocessing/
│   │   ├── export/
│   │   ├── flow/
│   │   ├── enhanced_dicom/
│   │   ├── cardiac/
│   │   └── coordinate/
│   └── ui/                     # Qt6 UI 컴포넌트
│       ├── panels/
│       ├── dialogs/
│       └── widgets/
├── src/                        # 구현 파일 (include/ 구조를 미러링)
│   ├── app/main.cpp
│   ├── core/
│   ├── services/
│   └── ui/
├── tests/
│   ├── unit/
│   ├── integration/
│   └── test_utils/
├── docs/                       # 문서 (PRD, SRS, SDS, Architecture)
└── benchmarks/                 # 성능 벤치마크
```

### 의존성 다이어그램

```
dicom_viewer (실행 파일)
    │
    ├── dicom_viewer_ui ─────── Qt6 (Widgets, OpenGL, Concurrent)
    │   ├── render_service ──── VTK (RenderingVolume, InteractionWidgets)
    │   ├── segmentation_service ── ITK (Segmentation, LevelSets)
    │   ├── measurement_service
    │   ├── preprocessing_service ── ITK (Filtering), FFTW3
    │   ├── pacs_service ────── pacs_system (kcenon 에코시스템)
    │   ├── export_service
    │   ├── flow_service
    │   ├── cardiac_service
    │   ├── enhanced_dicom_service ── GDCM
    │   └── coordinate_service
    │
    ├── dicom_viewer_core ───── ITK (IO, GDCM), zlib
    │
    └── kcenon 에코시스템
        ├── pacs_system (DICOM 네트워크)
        ├── thread_system (스레드 풀)
        ├── logger_system (로깅)
        └── common_system (공유 인터페이스)
```

---

## 코어 컴포넌트

### 1. DicomLoader

**역할**: DICOM 파일 파싱 및 시리즈 조립

**주요 클래스**:
- `DicomLoader`: 파일 및 디렉토리 스캐닝, 메타데이터 추출
- `SeriesBuilder`: 정렬된 슬라이스를 3D ITK 볼륨으로 조립

**담당 기능**:
- DICOM 태그 파싱 (환자 정보, study/series 메타데이터, 픽셀 데이터)
- 위치와 방향 기준으로 슬라이스 정렬
- 시리즈 일관성 검증 (간격, 방향)
- 정렬된 슬라이스로부터 `itk::Image<short, 3>` 볼륨 생성

### 2. ImageConverter

**역할**: ITK와 VTK 이미지 표현 간의 브릿지

**주요 동작**:
- `itk::Image`에서 `vtkImageData`로 변환
- 픽셀 타입 처리 (8비트, 16비트, float)
- 좌표계 정렬 (ITK LPS에서 VTK RAS로)

### 3. HounsfieldConverter

**역할**: CT 픽셀 값을 Hounsfield Unit으로 매핑

**기능**:
- Rescale slope/intercept 적용
- 조직 유형 분류 (air, fat, water, soft tissue, bone)
- 저장값 역변환 지원

### 4. TransferSyntaxDecoder

**역할**: DICOM transfer syntax 지원

**지원 구문**:
- Implicit/Explicit VR Little Endian
- JPEG Baseline, JPEG Lossless (Processes 14, 18)
- JPEG 2000 (무손실/손실)
- JPEG-LS (무손실/근무손실)
- RLE Lossless

### 5. ProjectManager

**역할**: 프로젝트 파일 영속화 (.flo 포맷)

**기능**:
- ZIP 기반 컨테이너에 NRRD 이미지 데이터 저장
- 세그멘테이션 마스크 직렬화
- 측정값 및 어노테이션 영속화

---

## 서비스 레이어

각 서비스는 별도의 CMake 정적 라이브러리로 빌드되어, 독립적인 테스트와 명확한 의존성 경계를 제공합니다.

### Render Service

VTK 기반 시각화를 제공하는 6개의 렌더링 컴포넌트.

| 컴포넌트 | 설명 |
|---------|------|
| `VolumeRenderer` | GPU ray casting(CPU 폴백 지원), transfer function 프리셋 |
| `SurfaceRenderer` | Marching Cubes 등표면 추출 |
| `MPRRenderer` | Axial/Coronal/Sagittal 슬라이스 동기화 뷰 |
| `ObliquResliceRenderer` | 임의 각도 리슬라이싱 |
| `HemodynamicOverlayRenderer` | 볼륨 위 4D Flow 오버레이 |
| `StreamlineOverlayRenderer` | 섬유 추적 시각화 |
| `TransferFunctionManager` | 색상/투명도 프리셋 관리, 커스텀 프리셋 지원 |

### Segmentation Service

인터랙티브 및 자동 세그멘테이션을 위한 16개 컴포넌트.

| 컴포넌트 | 설명 |
|---------|------|
| `ThresholdSegmenter` | Otsu 자동 임계값 설정 |
| `RegionGrowingSegmenter` | Confidence connected 영역 성장 |
| `LevelSetSegmenter` | Geodesic active contour (GAC) |
| `WatershedSegmenter` | 형태학적 워터셰드 |
| `ManualSegmentationController` | 브러시/지우개/채우기 도구 |
| `LabelManager` | 다중 라벨 관리 및 색상 할당 |
| `MorphologicalProcessor` | 열림, 닫힘, 고립점 제거 |
| `SliceInterpolator` | 수동 세그멘테이션 간 갭 채우기 |
| `CenterlineTracer` | 혈관 중심선 추출 |
| `LevelTracingTool` | 특정 강도 레벨의 등고선 추적 |
| `HollowTool` | 세그멘트된 구조물 내부 비우기 |
| `MaskSmoother` | 라벨 맵에 가우시안 스무딩 적용 |
| `PhaseTracker` | 심장 위상 간 마스크 전파 |
| `SegmentationCommand` | 실행 취소/다시 실행 프레임워크 (Command 패턴) |
| `SnapshotCommand` | RLE 압축 실행 취소 히스토리 |
| `LabelMapOverlay` | 세그멘테이션 오버레이 시각화 |

### Measurement Service

| 컴포넌트 | 설명 |
|---------|------|
| `LinearMeasurementTool` | 거리 및 각도 측정 |
| `AreaMeasurementTool` | 원형, 타원형, 자유형 ROI 면적 |
| `VolumeCalculator` | 세그멘트된 영역 체적 (mm3, cm3, mL) |
| `ROIStatistics` | Mean, StdDev, Min/Max, 히스토그램 |
| `ShapeAnalyzer` | 표면적, 구형도, 질량 중심 |

### Preprocessing Service

| 컴포넌트 | 설명 |
|---------|------|
| `GaussianSmoother` | 가우시안 블러를 통한 노이즈 저감 |
| `AnisotropicDiffusionFilter` | 에지 보존 스무딩 |
| `N4BiasCorrector` | MRI B1 바이어스 필드 보정 (FFTW 필요) |
| `IsotropicResampler` | 복셀 간격 정규화 |
| `HistogramEqualizer` | 대비 향상 |

### PACS Service

kcenon pacs_system 라이브러리 기반의 DICOM 네트워크 운영.

| 컴포넌트 | 설명 |
|---------|------|
| `DicomEchoScu` | C-ECHO 검증 |
| `DicomFindScu` | C-FIND 쿼리 (patient/study/series/image) |
| `DicomMoveScu` | C-MOVE 이미지 검색 |
| `DicomStoreScp` | C-STORE SCP 서버 (이미지 수신) |
| `PacsConfigManager` | 서버 설정 영속화 (QSettings) |

### Export Service

| 컴포넌트 | 설명 |
|---------|------|
| `DataExporter` | 메타데이터 포함 NRRD 이미지 내보내기 |
| `MeshExporter` | STL (바이너리/ASCII), OBJ, PLY 메시 내보내기 |
| `ReportGenerator` | PDF/HTML 의료 보고서 |
| `MeasurementSerializer` | JSON/CSV 측정 데이터 |
| `DicomSRWriter` | DICOM Structured Report 생성 |
| `EnsightExporter` | CFD 도구용 Ensight Gold 포맷 |
| `MatlabExporter` | MATLAB v5 .mat 파일 포맷 |
| `VideoExporter` | OGG Theora 비디오 생성 |

### Flow Service

벤더별 파싱을 포함한 4D Flow MRI 분석.

| 컴포넌트 | 설명 |
|---------|------|
| `FlowDicomParser` | 4D Flow 시리즈 감지 및 구성 |
| `VelocityFieldAssembler` | 속도 인코딩 컴포넌트로부터 벡터 필드 생성 |
| `PhaseCorrector` | 앨리어싱 해제 및 와전류 보정 |
| `TemporalNavigator` | 4D 시퀀스의 위상/프레임 탐색 |
| `FlowVisualizer` | 유선, 경로선, 글리프 렌더링 |
| `FlowQuantifier` | 유량, 속도 곡선, 압력 구배 |
| `VesselAnalyzer` | WSS, OSI, 난류 운동 에너지(TKE) |

**벤더별 파서** (Strategy 패턴):
- `SiemensFlowParser`: Siemens VENC 추출
- `PhilipsFlowParser`: Philips VENC 추출
- `GEFlowParser`: GE VENC 추출

### Enhanced DICOM Service

GDCM을 사용한 Multi-frame Enhanced DICOM IOD 파싱.

| 컴포넌트 | 설명 |
|---------|------|
| `EnhancedDicomParser` | Multi-frame IOD 감지 및 파싱 |
| `FunctionalGroupParser` | 공유 및 프레임별 메타데이터 추출 |
| `FrameExtractor` | 픽셀 데이터로부터 개별 프레임 추출 |
| `DimensionIndexSorter` | 공간/시간 차원별 프레임 정렬 |
| `SeriesClassifier` | Single-frame vs multi-frame 분류 |

### Cardiac Service

| 컴포넌트 | 설명 |
|---------|------|
| `CardiacPhaseDetector` | ECG 게이트 위상 분리 |
| `CalciumScorer` | Agatston, 체적, 질량 칼슘 스코어링 |
| `CoronaryCenterlineExtractor` | 혈관 중심선 추적 |
| `CurvedPlanarReformatter` | 곡면 평면 재구성(CPR) |
| `CineOrganizer` | Cine 시퀀스 구성 (SA, 2CH, 3CH, 4CH) |

### Coordinate Service

| 컴포넌트 | 설명 |
|---------|------|
| `MPRCoordinateTransformer` | 크로스헤어 동기화를 포함한 통합 좌표계 |

---

## UI 레이어

### 컴포넌트 계층 구조

```
MainWindow
├── Central Widget
│   ├── ViewportWidget (1x1 또는 2x2 레이아웃)
│   │   ├── MPRViewWidget (Axial / Coronal / Sagittal)
│   │   ├── DRViewer (2D DR/CR 뷰잉)
│   │   └── Display3DController (볼륨 렌더 컨트롤)
│   └── ViewportLayoutManager (레이아웃 전환)
│
├── Dock Panels
│   ├── PatientBrowser (시리즈 목록, 분류 정보)
│   ├── ToolsPanel (측정 및 세그멘테이션 도구)
│   ├── SegmentationPanel (라벨 관리, 브러시 컨트롤)
│   ├── StatisticsPanel (ROI 통계 표시)
│   ├── OverlayControlPanel (세그멘테이션 표시/투명도)
│   ├── FlowToolPanel (4D Flow 컨트롤)
│   ├── ReportPanel (보고서 생성)
│   └── WorkflowPanel (S/P 모드, 위상 슬라이더)
│
├── Dialogs
│   ├── SettingsDialog (로그 레벨, UI 환경설정)
│   ├── PacsConfigDialog (PACS 서버 설정)
│   ├── QuantificationWindow (Flow/심장 계측)
│   ├── VideoExportDialog (비디오 내보내기 파라미터)
│   └── MaskWizard (다단계 세그멘테이션 마법사)
│
└── Widgets
    ├── PhaseSliderWidget (심장 위상 / 시간 프레임)
    ├── SPModeToggle (Single/Phase 모드 전환)
    ├── FlowGraphWidget (시계열 차트, QPainter)
    └── WorkflowTabBar (탭 기반 워크플로우 탐색)
```

### 주요 UI 패턴

- **Pimpl**: 모든 주요 UI 클래스가 ABI 안정성을 위해 포인터-구현체 패턴 사용
- **Dock Widgets**: 패널들이 Qt 도크 위젯으로 유연한 레이아웃 가능
- **Signal/Slot**: 컴포넌트 간 통신에 Qt signal/slot 메커니즘 사용
- **MVC 분리**: 컨트롤러(예: `MaskWizardController`, `Display3DController`)가 로직과 뷰를 분리

---

## 데이터 흐름

### DICOM 파일에서 렌더링까지

```
1. 파일 로딩
   DICOM 파일 --> DicomLoader.scanDirectory()
   --> SliceInfo[] (슬라이스별 메타데이터)

2. 시리즈 조립
   SliceInfo[] --> SeriesBuilder.buildVolume()
   --> itk::Image<short, 3> (정렬된 3D 볼륨)

3. Enhanced DICOM (multi-frame인 경우)
   Multi-frame 파일 --> EnhancedDicomParser
   --> FrameExtractor --> 프레임별 볼륨

4. 전처리 (선택)
   itk::Image --> GaussianSmoother / N4BiasCorrector
   --> 필터링된 itk::Image

5. 포맷 변환
   itk::Image --> ImageConverter --> vtkImageData

6. 볼륨 렌더링
   vtkImageData --> VolumeRenderer
   --> TransferFunction --> VTK 렌더 파이프라인

7. MPR 뷰
   vtkImageData --> MPRRenderer (3개 평면)
   --> MPRCoordinateTransformer (크로스헤어 동기화)
```

### 4D Flow 파이프라인

```
DICOM 파일 --> FlowDicomParser --> VendorParser (Siemens/Philips/GE)
--> PhaseCorrector --> VelocityFieldAssembler
--> FlowVisualizer (유선) + FlowQuantifier (계측)
--> VesselAnalyzer (WSS, OSI, TKE)
```

### 심장 파이프라인

```
Enhanced CT --> CardiacPhaseDetector --> 위상별 볼륨
--> CalciumScorer (Agatston 점수)
--> CoronaryCenterlineExtractor --> CurvedPlanarReformatter (CPR)
```

### 내보내기 파이프라인

```
볼륨/메시/측정값 --> DataExporter
--> NRRD, STL, OBJ, PLY, PDF, HTML, JSON, CSV,
    DICOM SR, Ensight, MATLAB .mat, OGG 비디오
```

---

## 설계 원칙

### 1. Pimpl (Pointer to Implementation)

모든 주요 클래스가 구현 세부사항 은닉, 컴파일 시간 단축, ABI 안정성을 위해 Pimpl 패턴을 사용합니다.

```cpp
class VolumeRenderer {
public:
    VolumeRenderer();
    ~VolumeRenderer();
    VolumeRenderer(VolumeRenderer&&) noexcept;
    VolumeRenderer& operator=(VolumeRenderer&&) noexcept;
    // 공개 API ...
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
```

### 2. std::expected 오류 처리 (C++23)

서비스 메서드가 명시적이고 예외 없는 오류 전파를 위해 `std::expected<T, std::string>`을 반환합니다.

### 3. Strategy 패턴

벤더별 로직(예: Flow DICOM 파싱)이 공통 인터페이스(`IVendorFlowParser`)와 벤더별 구현체를 가진 Strategy 패턴을 사용합니다.

### 4. Command 패턴

세그멘테이션 실행 취소/다시 실행이 Command 패턴을 사용합니다. `SnapshotCommand`는 메모리 효율적인 실행 취소 히스토리를 위해 RLE 압축된 라벨 맵을 저장합니다.

### 5. 서비스 레이어 격리

각 서비스는 자체 소스 및 헤더 디렉토리를 가진 별도의 CMake 정적 라이브러리입니다. 서비스들은 내부 구현 세부사항이 아닌 잘 정의된 인터페이스를 통해 통신합니다.

---

## 의존성

### 외부 라이브러리

| 라이브러리 | 버전 | 용도 |
|-----------|------|------|
| Qt6 | 6.5+ | UI 프레임워크 (Widgets, OpenGL, Concurrent) |
| VTK | 9.3+ | 3D 시각화 및 볼륨 렌더링 |
| ITK | 5.4+ | 의료 영상 처리 및 DICOM I/O |
| GDCM | - | 저수준 DICOM 태그 파싱 (Enhanced DICOM) |
| FFTW3 | - | N4 바이어스 필드 보정용 FFT |
| spdlog | - | 고성능 로깅 |
| fmt | - | 문자열 포매팅 |
| nlohmann_json | - | JSON 직렬화 |
| zlib | - | 압축 (프로젝트 파일) |
| Google Test | - | 테스트 프레임워크 |

### kcenon 에코시스템

| 라이브러리 | 용도 |
|-----------|------|
| pacs_system | DICOM 네트워크 운영 (SCU/SCP) |
| thread_system | 스레드 풀 및 작업 스케줄링 |
| logger_system | 구조화된 로깅 인프라 |
| common_system | 공유 인터페이스 (IExecutor, Result<T>) |

---

## 성능 목표

| 지표 | 목표 | 조건 |
|------|------|------|
| CT 시리즈 로딩 | <= 3초 | 512 x 512 x 300 볼륨 |
| 볼륨 렌더링 | >= 30 FPS | 인터랙티브 회전/줌 |
| MPR 슬라이스 전환 | <= 100ms | 마우스 휠 스크롤 |
| 메모리 사용량 | <= 2 GB | 1 GB 볼륨 하나 로드 시 |
| 애플리케이션 시작 | <= 5초 | 콜드 스타트 |

---

## 테스트 인프라

**3071개 이상의 테스트**가 유닛, 통합, 벤치마크 카테고리에 걸쳐 존재합니다.

| 카테고리 | 수량 | 예시 |
|---------|------|------|
| 유닛 | 80+ 실행 파일 | 각 서비스 모듈마다 전용 테스트 |
| 통합 | 4개 실행 파일 | Flow 파이프라인, 심장 파이프라인, 시리즈 로딩 |
| 벤치마크 | 2개 실행 파일 | 성능 및 렌더링 벤치마크 |

모든 테스트는 CTest 통합을 위해 `gtest_discover_tests`와 함께 Google Test를 사용합니다.

---

**날짜**: 2026-02-23
**버전**: 0.7.0-pre

# DICOM Viewer

> **고성능 의료 영상 뷰어** - CT/MRI 3D 볼륨 렌더링 및 MPR 뷰 지원

[![Version](https://img.shields.io/badge/version-0.2.0--pre-orange)](https://github.com)
[![C++](https://img.shields.io/badge/C++-20-blue.svg)](https://isocpp.org)
[![License](https://img.shields.io/badge/license-BSD--3--Clause-green.svg)](LICENSE)

## 개요

**DICOM Viewer**는 영상의학과 전문의와 의료 영상 연구자를 위한 고성능 의료 영상 뷰어입니다. CT와 MRI 영상의 3D 볼륨 렌더링 및 MPR(Multi-Planar Reconstruction) 뷰를 우선적으로 지원하며, DR/CR 영상의 기본 2D 뷰잉 기능을 제공합니다.

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

## 주요 기능

### CT/MRI 3D 시각화 (P0 - Critical)

| 기능 | 설명 |
|------|------|
| **볼륨 렌더링** | GPU 가속 Ray Casting, Transfer Function 편집, 프리셋 (CT Bone, Soft Tissue, Lung, Angio) |
| **MPR 뷰** | Axial, Coronal, Sagittal 동시 표시, 크로스헤어 동기화, Oblique 리슬라이싱 |
| **표면 렌더링** | Marching Cubes 등표면 추출, 다중 표면 동시 렌더링, STL/PLY 내보내기 |
| **볼륨 클리핑** | 인터랙티브 클리핑 박스로 내부 구조 확인 |

### 영역 세그먼테이션 (P1 - High)

| 분할 방법 | 설명 |
|-----------|------|
| **자동 분할** | Otsu 임계값, Region Growing, Confidence Connected |
| **반자동 분할** | Level Set (Geodesic Active Contour), Watershed |
| **수동 분할** | 브러시, 지우개, 채우기, 다각형, 자유곡선 도구 |
| **후처리** | Opening, Closing, Fill Holes, Island Removal |

### 영역 측정 및 분석 (P1 - High)

- **선형 측정**: 거리 (2점), 각도 (3점), Cobb 각도
- **면적 측정**: 원형/타원형, 다각형, 자유곡선 ROI
- **볼륨 측정**: 분할 영역 볼륨 계산 (mm³, cm³, mL)
- **ROI 통계**: Mean, StdDev, Min/Max, 히스토그램
- **고급 분석**: 표면적, 구형도, 무게중심

### PACS 연동 (P1 - High)

| 서비스 | 기능 |
|--------|------|
| **C-ECHO** | 연결 테스트 |
| **C-FIND** | Patient/Study/Series/Image 레벨 검색 |
| **C-MOVE** | 영상 가져오기 |
| **C-STORE SCP** | 영상 수신 |

### DR/CR 2D 뷰잉 (P2 - Medium)

- Window/Level 조절 (마우스 드래그)
- 확대/축소, 팬, 회전
- 좌우/상하 반전
- 그리드 뷰 (다중 영상 비교)

## 기술 스택

| Component | Technology | Version |
|-----------|------------|---------|
| **언어** | C++ | C++20 |
| **빌드** | CMake | 3.20+ |
| **GUI** | Qt | 6.5+ |
| **영상 처리** | ITK | 5.4+ |
| **시각화** | VTK | 9.3+ |
| **DICOM** | pacs_system | Latest |

## 아키텍처

DICOM Viewer는 **4-Layer 아키텍처**를 채택하여 관심사 분리와 유지보수성을 극대화합니다.

```
┌─────────────────────────────────────────────────────────────────────────┐
│  Presentation Layer (Qt6)                                               │
│  • MainWindow, ViewportWidget, ToolsPanel, PatientBrowser               │
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

## 성능 목표

| 메트릭 | 목표 | 조건 |
|--------|------|------|
| CT 시리즈 로딩 | ≤ 3초 | 512×512×300 볼륨 |
| 볼륨 렌더링 FPS | ≥ 30 FPS | 인터랙티브 회전/줌 |
| MPR 슬라이스 전환 | ≤ 100ms | 마우스 휠 스크롤 |
| 메모리 사용량 | ≤ 2GB | 1GB 볼륨 데이터 기준 |
| 애플리케이션 시작 | ≤ 5초 | Cold Start |

## 지원 환경

### 운영체제

- macOS 12+ (Monterey)
- Windows 10+ (64-bit)
- Ubuntu 20.04 LTS+

### 하드웨어 요구사항

- **GPU**: OpenGL 4.1+ 지원 (VRAM 2GB+, 4GB 권장)
- **RAM**: 8GB+ (16GB 권장)
- **저장공간**: 1GB+ (설치용)

### 지원 DICOM 포맷

| Transfer Syntax | 지원 |
|-----------------|------|
| Implicit VR Little Endian | ✅ |
| Explicit VR Little Endian | ✅ |
| JPEG Baseline | ✅ |
| JPEG Lossless | ✅ |
| JPEG 2000 | ✅ |
| JPEG-LS | ✅ |
| RLE Lossless | ✅ |

## 시작하기

### 의존성 설치

```bash
# macOS (Homebrew)
brew install itk vtk qt@6

# vcpkg 사용 시
vcpkg install itk[vtk] vtk[qt] qt6
```

### 빌드

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

### 실행

```bash
./dicom_viewer
```

## 프로젝트 구조

```
dicom_viewer/
├── docs/                           # 문서
│   ├── PRD.md                      # 제품 요구사항 문서
│   ├── SRS.md                      # 소프트웨어 요구사항 명세서
│   ├── SDS.md                      # 소프트웨어 설계 명세서
│   └── reference/                  # 기술 참조 문서
│       ├── 01-itk-overview.md      # ITK 아키텍처 및 API
│       ├── 02-vtk-overview.md      # VTK 아키텍처 및 API
│       ├── 03-itk-vtk-integration.md  # ITK-VTK 통합 가이드
│       ├── 04-dicom-pipeline.md    # DICOM 처리 파이프라인
│       └── 05-pacs-integration.md  # pacs_system 연동 가이드
├── src/                            # 소스 코드 (예정)
│   ├── core/                       # 핵심 데이터 구조
│   ├── service/                    # 서비스 레이어
│   ├── controller/                 # 컨트롤러 레이어
│   └── ui/                         # Qt UI 컴포넌트
├── LICENSE
└── README.md
```

## 문서

| 문서 | 설명 |
|------|------|
| [PRD](docs/PRD.md) | 제품 요구사항 문서 - 비전, 목표, 기능 요구사항 |
| [SRS](docs/SRS.md) | 소프트웨어 요구사항 명세서 - 상세 기술 요구사항 |
| [SDS](docs/SDS.md) | 소프트웨어 설계 명세서 - 아키텍처 및 모듈 설계 |
| [Reference](docs/reference/README.md) | 기술 참조 문서 - ITK, VTK, pacs_system |

## 로드맵

```
v0.1 → v0.2 → v0.3 (MVP) → v0.4 → v0.5 → v1.0
───────────────────────────────────────────────
Phase 1        Phase 2: Core     Phase 3
Foundation     Features          Enhancement
```

| 버전 | 목표 | 주요 기능 |
|------|------|-----------|
| v0.3 (MVP) | CT/MRI Viewer | DICOM 로딩, 볼륨/표면 렌더링, MPR, 프리셋 |
| v0.4 | Core Features | PACS 연동, 측정 도구, 기본 분할, 전처리 |
| v0.5 | Enhancement | DR/CR 뷰잉, 고급 분할, 리포트 생성 |
| v1.0 | Release | 안정화, 사용자 설정, 레이아웃 저장 |

## 대상 사용자

| 사용자 | 주요 요구사항 |
|--------|--------------|
| **영상의학과 전문의** | CT/MRI 판독, 3D 재구성, 측정/정량 분석 |
| **연구자/개발자** | 알고리즘 검증, 분할 연구, API 접근성 |
| **방사선사/기사** | DR/CR 확인, PACS 연동, 간편한 조작 |

## 관련 프로젝트

- [pacs_system](https://github.com) - DICOM 처리 및 PACS 기능 (kcenon 에코시스템)
- [3D Slicer](https://www.slicer.org/) - 의료 영상 분석 플랫폼
- [MITK](https://www.mitk.org/) - ITK+VTK 통합 프레임워크

## 참고 자료

- [ITK Documentation](https://itk.org/)
- [VTK Documentation](https://vtk.org/)
- [DICOM Standard](https://dicom.nema.org/)
- [Qt6 Documentation](https://doc.qt.io/)

## 라이선스

BSD 3-Clause License - 자세한 내용은 [LICENSE](LICENSE) 파일을 참조하세요.

---

> **Note**: 현재 버전(v0.x.x)은 Pre-release로 개발 중인 상태입니다. 정식 릴리스는 v1.0.0부터 시작됩니다.

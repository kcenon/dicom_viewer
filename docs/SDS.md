# DICOM Viewer - Software Design Specification (SDS)

> **Version**: 0.7.0
> **Created**: 2025-12-31
> **Last Updated**: 2026-02-20
> **Status**: Draft (Pre-release)
> **Author**: Development Team
> **Based on**: [SRS v0.7.0](SRS.md), [PRD v0.6.0](PRD.md)

---

## Document Information

### Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 0.1.0 | 2025-12-31 | Development Team | Initial SDS based on SRS 0.1.0 |
| 0.2.0 | 2025-12-31 | Development Team | Added segmentation and measurement module design |
| 0.3.0 | 2026-02-11 | Development Team | Replaced DCMTK with pacs_system for DICOM network operations; version sync with build system |
| 0.4.0 | 2026-02-11 | Development Team | Fixed SRS-FR traceability references; aligned with SRS v0.4.0 |
| 0.5.0 | 2026-02-11 | Development Team | Added SDS-MOD-007 Flow Analysis Module with 4D Flow MRI support |
| 0.6.0 | 2026-02-12 | Development Team | Added SDS-MOD-008 (Enhanced DICOM Module, 4 components), SDS-MOD-009 (Cardiac CT Analysis Module, 5 components); updated traceability matrices for SRS-FR-049~053 |
| 0.7.0 | 2026-02-20 | Development Team | Updated implementation statuses for MOD-007/008/009 to Implemented; added SDS-MOD-010 Export Service Module (8 components); expanded MOD-002 with advanced segmentation tools, MOD-003 with hemodynamic renderers, MOD-006 with 20 additional UI components; updated traceability matrices |

### Referenced Documents

| Document ID | Title | Location |
|-------------|-------|----------|
| PRD-001 | Product Requirements Document | [PRD.md](PRD.md) |
| SRS-001 | Software Requirements Specification | [SRS.md](SRS.md) |
| REF-001 | ITK Overview | [reference/01-itk-overview.md](reference/01-itk-overview.md) |
| REF-002 | VTK Overview | [reference/02-vtk-overview.md](reference/02-vtk-overview.md) |
| REF-003 | ITK-VTK Integration | [reference/03-itk-vtk-integration.md](reference/03-itk-vtk-integration.md) |
| REF-004 | DICOM Pipeline | [reference/04-dicom-pipeline.md](reference/04-dicom-pipeline.md) |
| REF-005 | PACS Integration | [reference/05-pacs-integration.md](reference/05-pacs-integration.md) |
| REF-006 | GUI Framework Comparison | [reference/06-gui-framework-comparison.md](reference/06-gui-framework-comparison.md) |
| REF-007 | Remote Visualization | [reference/07-remote-visualization.md](reference/07-remote-visualization.md) |

### Design Element ID Convention

- **SDS-ARCH-XXX**: Architectural Design
- **SDS-MOD-XXX**: Module Design
- **SDS-CLS-XXX**: Class Design
- **SDS-IF-XXX**: Interface Design
- **SDS-DATA-XXX**: Data Design
- **SDS-SEQ-XXX**: Sequence Design

---

## 1. Introduction

### 1.1 Purpose

This document is the Software Design Specification (SDS) for the DICOM Viewer software. It provides architecture, module, class, interface, and data design for implementing the software requirements defined in the SRS.

### 1.2 Scope

This document covers the following design aspects:
- System architecture design
- Detailed module design
- Class diagrams and relationships
- Interface definitions
- Data structures and flow
- Sequence diagrams
- Traceability matrix to PRD/SRS

### 1.3 Design Principles

| Principle | Description | Application |
|-----------|-------------|-------------|
| **Separation of Concerns** | Separate responsibilities | Layered architecture, MVC pattern |
| **Dependency Injection** | Inject dependencies | Service layer |
| **Interface Segregation** | Segregate interfaces | Small, focused interfaces |
| **Open/Closed** | Open for extension, closed for modification | Extensible plugin architecture |
| **Single Responsibility** | One responsibility per class | Clear role for each class |

---

## 2. Architectural Design

### SDS-ARCH-001: System Architecture Overview

**Traces to**: SRS System Overview, PRD Section 6

> **Note**: Diagrams are provided in both **Mermaid format** (auto-rendered in GitHub/GitLab) and **ASCII format** (universal compatibility).

#### Mermaid Version (Auto-rendered in GitHub/GitLab)

```mermaid
%%{init: {'theme': 'base', 'themeVariables': { 'primaryColor': '#e1f5fe', 'primaryTextColor': '#01579b', 'primaryBorderColor': '#0288d1', 'lineColor': '#0288d1', 'secondaryColor': '#fff3e0', 'tertiaryColor': '#f3e5f5'}}}%%
flowchart TB
    subgraph PresentationLayer["🖥️ Presentation Layer (Qt6)"]
        direction LR
        MainWindow["MainWindow<br/>• Menu Bar<br/>• Tool Bar<br/>• Status Bar"]
        ViewportWidget["ViewportWidget<br/>• 3D Viewport<br/>• MPR Views<br/>• 2D View"]
        ToolsPanel["ToolsPanel<br/>• Preset Panel<br/>• Measure Panel<br/>• Segment Panel"]
        PatientBrowser["PatientBrowser"]
        PACSQueryDlg["PACSQueryDlg"]
        SettingsDlg["SettingsDlg"]
    end

    subgraph ControllerLayer["⚙️ Controller Layer (stub — not yet implemented)"]
        direction LR
        ViewerController["ViewerController"]
        LoadingCtrl["LoadingController"]
        RenderCtrl["RenderingController"]
        ToolCtrl["ToolController"]
        ViewerController --- LoadingCtrl
        ViewerController --- RenderCtrl
        ViewerController --- ToolCtrl
    end

    subgraph ServiceLayer["🔧 Service Layer (direct component access)"]
        direction LR
        ImageServices["Image Services<br/>• DicomLoader<br/>• SeriesBuilder<br/>• ImageConverter"]
        RenderServices["Render Services<br/>• VolumeRenderer<br/>• SurfaceRenderer<br/>• MPRRenderer"]
        MeasureServices["Measurement Services<br/>• LinearMeasurementTool<br/>• AreaMeasurementTool<br/>• ROIStatistics"]
        PACSServices["PACS Services<br/>• DicomFindSCU<br/>• DicomMoveSCU<br/>• DicomStoreSCP"]
    end

    subgraph DataLayer["💾 Data Layer"]
        direction LR
        ImageData["ImageData<br/>• ITK Image<br/>• VTK Image"]
        DicomData["DicomData<br/>• pacs_system<br/>• Dataset"]
        MetaData["MetaData<br/>• Patient/Study<br/>• Series"]
        SegmentData["SegmentData<br/>• LabelMap<br/>• ROI"]
    end

    subgraph ExternalLayer["📚 External Libraries"]
        direction LR
        pacs_system["pacs_system<br/>DICOM Core"]
        ITK["ITK<br/>Processing"]
        VTK["VTK<br/>Visualization"]
        Qt6["Qt6<br/>GUI Framework"]
    end

    PresentationLayer --> ControllerLayer
    ControllerLayer --> ServiceLayer
    ServiceLayer --> DataLayer
    DataLayer --> ExternalLayer

    style PresentationLayer fill:#e3f2fd,stroke:#1976d2
    style ControllerLayer fill:#fff3e0,stroke:#f57c00
    style ServiceLayer fill:#e8f5e9,stroke:#388e3c
    style DataLayer fill:#fce4ec,stroke:#c2185b
    style ExternalLayer fill:#f3e5f5,stroke:#7b1fa2
```

#### ASCII Version (Universal Compatibility)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       DICOM Viewer System Architecture                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │                        Presentation Layer (Qt6)                          ││
│  │                                                                           ││
│  │  ┌───────────────┐  ┌────────────────┐  ┌────────────────┐              ││
│  │  │  MainWindow   │  │ ViewportWidget │  │   ToolsPanel   │              ││
│  │  │               │  │                │  │                │              ││
│  │  │ • Menu Bar    │  │ • 3D Viewport  │  │ • Preset Panel │              ││
│  │  │ • Tool Bar    │  │ • MPR Views    │  │ • Measure Panel│              ││
│  │  │ • Status Bar  │  │ • 2D View      │  │ • Segment Panel│              ││
│  │  │ • Dock Areas  │  │ (QVTK Widget)  │  │ • ROI Panel    │              ││
│  │  └───────────────┘  └────────────────┘  └────────────────┘              ││
│  │                                                                           ││
│  │  ┌───────────────┐  ┌────────────────┐  ┌────────────────┐              ││
│  │  │PatientBrowser │  │ PACSQueryDlg   │  │  SettingsDlg   │              ││
│  │  └───────────────┘  └────────────────┘  └────────────────┘              ││
│  │                                                                           ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                      │                                        │
│                                      ↓                                        │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │                       Controller Layer                                    ││
│  │                                                                           ││
│  │  ┌───────────────────────────────────────────────────────────────────┐  ││
│  │  │                      ViewerController                              │  ││
│  │  │                                                                     │  ││
│  │  │  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌────────────┐│  ││
│  │  │  │   Loading    │ │  Rendering   │ │    Tool      │ │  Network   ││  ││
│  │  │  │  Controller  │ │  Controller  │ │  Controller  │ │ Controller ││  ││
│  │  │  └──────────────┘ └──────────────┘ └──────────────┘ └────────────┘│  ││
│  │  └───────────────────────────────────────────────────────────────────┘  ││
│  │                                                                           ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                      │                                        │
│                                      ↓                                        │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │                         Service Layer                                     ││
│  │                                                                           ││
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌───────────────────┐  ││
│  │  │   Image     │ │   Render    │ │ Measurement │ │    Network        │  ││
│  │  │   Service   │ │   Service   │ │   Service   │ │    Service        │  ││
│  │  │             │ │             │ │             │ │                   │  ││
│  │  │ • Loading   │ │ • Volume    │ │ • Distance  │ │ • C-FIND          │  ││
│  │  │ • Preproc   │ │ • Surface   │ │ • Area/Vol  │ │ • C-MOVE          │  ││
│  │  │ • Segment   │ │ • MPR       │ │ • Statistics│ │ • C-STORE         │  ││
│  │  │ • Convert   │ │ • 2D View   │ │ • ROI Mgmt  │ │ • Echo            │  ││
│  │  └─────────────┘ └─────────────┘ └─────────────┘ └───────────────────┘  ││
│  │                                                                           ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                      │                                        │
│                                      ↓                                        │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │                           Data Layer                                      ││
│  │                                                                           ││
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌───────────────────┐  ││
│  │  │  ImageData  │ │  DicomData  │ │  MetaData   │ │  SegmentData      │  ││
│  │  │             │ │             │ │             │ │                   │  ││
│  │  │ • ITK Image │ │ • pacs_sys  │ │ • Patient   │ │ • LabelMap        │  ││
│  │  │ • VTK Image │ │   Dataset   │ │ • Study     │ │ • ROI Collection  │  ││
│  │  │ • Bridge    │ │             │ │ • Series    │ │ • Measurements    │  ││
│  │  └─────────────┘ └─────────────┘ └─────────────┘ └───────────────────┘  ││
│  │                                                                           ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                      │                                        │
│                                      ↓                                        │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │                      External Libraries Layer                             ││
│  │                                                                           ││
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌───────────────────┐  ││
│  │  │ pacs_system │ │     ITK     │ │     VTK     │ │       Qt6         │  ││
│  │  │             │ │             │ │             │ │                   │  ││
│  │  │ DICOM Core  │ │ Processing  │ │ Visualiz.   │ │ GUI Framework     │  ││
│  │  │ Network     │ │ Segmentation│ │ Rendering   │ │ Widgets           │  ││
│  │  │ Codecs      │ │ Registration│ │ Interaction │ │ OpenGL            │  ││
│  │  └─────────────┘ └─────────────┘ └─────────────┘ └───────────────────┘  ││
│  │                                                                           ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                                                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

### SDS-ARCH-002: Layer Responsibilities

**Traces to**: SRS-FR-001 through SRS-FR-048

| Layer | Responsibility | Key Technologies | Dependencies |
|-------|---------------|------------------|--------------|
| **Presentation** | UI rendering, user input handling, temporal navigation | Qt6, QVTKOpenGLNativeWidget | Controller |
| **Controller** | Request coordination, event handling | C++ | Service |
| **Service** | Business logic, image processing, flow analysis | ITK, VTK | Data |
| **Data** | Data storage, conversion, management | pacs_system | External Libs |
| **External Libs** | Foundation functionality | ITK, VTK, Qt6, pacs_system | OS |

---

### SDS-ARCH-003: Module Dependency Graph

**Traces to**: PRD Section 6.2

#### Mermaid Version

```mermaid
%%{init: {'theme': 'base', 'themeVariables': { 'primaryColor': '#e8f5e9'}}}%%
graph TB
    subgraph Application["📦 Application Layer"]
        App[dicom_viewer_app]
    end

    subgraph UI["🖥️ UI Layer"]
        UILib[dicom_viewer_ui]
    end

    subgraph Controller["⚙️ Controller Layer"]
        Ctrl[dicom_viewer_controller]
    end

    subgraph Services["🔧 Service Layer"]
        ImgSvc[image_service]
        RndSvc[render_service]
        NetSvc[network_service]
        MsrSvc[measurement_service]
        FlwSvc[flow_service]
    end

    subgraph Core["🔩 Core Layer"]
        CoreLib[dicom_viewer_core]
    end

    subgraph External["📚 External Libraries"]
        PACS[pacs_system]
        ITKLib[ITK]
        VTKLib[VTK]
        QtLib[Qt6]
    end

    App --> UILib
    App --> Ctrl
    UILib --> Ctrl

    Ctrl --> ImgSvc
    Ctrl --> RndSvc
    Ctrl --> NetSvc
    Ctrl --> MsrSvc
    Ctrl --> FlwSvc

    ImgSvc --> CoreLib
    RndSvc --> CoreLib
    NetSvc --> CoreLib
    MsrSvc --> CoreLib
    FlwSvc --> CoreLib
    FlwSvc --> ImgSvc
    FlwSvc --> RndSvc

    CoreLib --> PACS
    CoreLib --> ITKLib
    CoreLib --> VTKLib

    PACS --> QtLib
    ITKLib --> QtLib
    VTKLib --> QtLib

    style Application fill:#bbdefb,stroke:#1976d2
    style UI fill:#c8e6c9,stroke:#388e3c
    style Controller fill:#fff9c4,stroke:#fbc02d
    style Services fill:#ffccbc,stroke:#e64a19
    style Core fill:#d1c4e9,stroke:#7b1fa2
    style External fill:#f5f5f5,stroke:#616161
```

#### ASCII Version

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         Module Dependency Graph                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│   dicom_viewer_app  ──────────────────────────────────────────────────┐      │
│         │                                                              │      │
│         ↓                                                              │      │
│   dicom_viewer_ui                                                      │      │
│         │                                                              │      │
│         ↓                                                              │      │
│   dicom_viewer_controller  ←───────────────────────────────────────────┘      │
│         │                                                                     │
│         ├──────────────┬──────────────┬──────────────┬──────────────┐        │
│         ↓              ↓              ↓              ↓              ↓        │
│   image_service  render_service  network_svc  measurement_svc  flow_service │
│         │              │              │              │              │        │
│         │              │              │              │         ┌────┘        │
│         │              │←─────────────│──────────────│─────────│             │
│         │←─────────────│──────────────│──────────────│─────────┘             │
│         └──────────────┴──────────────┴──────────────┘                      │
│                                   │                                          │
│                                   ↓                                          │
│                         dicom_viewer_core                                    │
│                                   │                                          │
│         ┌─────────────────────────┼─────────────────────────┐               │
│         ↓                         ↓                         ↓               │
│    pacs_system                   ITK                       VTK              │
│         │                         │                         │               │
│         └─────────────────────────┴─────────────────────────┘               │
│                                   │                                          │
│                                   ↓                                          │
│                                  Qt6                                         │
│                                                                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

### SDS-ARCH-004: Data Flow Architecture

**Traces to**: SRS-FR-001, SRS-FR-002, SRS-FR-005

#### Mermaid Version

```mermaid
%%{init: {'theme': 'base'}}%%
flowchart LR
    subgraph Input["📁 Input"]
        DICOM["DICOM Files<br/>(*.dcm)"]
    end

    subgraph Parse["🔍 Parsing"]
        PACS["pacs_system<br/>dicom_dataset"]
    end

    subgraph Process["🔧 Processing"]
        ITK["ITK Image<br/>itk::Image&lt;short,3&gt;"]
        Filter["Filtering"]
        Segment["Segmentation"]
        Register["Registration"]
    end

    subgraph Visualize["🖼️ Visualization"]
        VTK["vtkImageData"]
        Volume["Volume Rendering"]
        Surface["Surface Rendering"]
        MPR["MPR Views"]
        View2D["2D View"]
    end

    subgraph Output["📊 Output"]
        Render["Render<br/>Window"]
    end

    subgraph Storage["💾 Metadata"]
        Meta["Patient | Study | Series | Image"]
    end

    DICOM -->|"pacs::dicom_file::open()"| PACS
    PACS -->|"PACSToITK Adapter"| ITK
    PACS -.->|"Extract"| Meta

    ITK --> Filter
    ITK --> Segment
    ITK --> Register

    ITK -->|"ITKVtkGlue"| VTK

    VTK --> Volume
    VTK --> Surface
    VTK --> MPR
    VTK --> View2D

    Volume --> Render
    Surface --> Render
    MPR --> Render
    View2D --> Render

    style Input fill:#e3f2fd,stroke:#1976d2
    style Parse fill:#fff3e0,stroke:#f57c00
    style Process fill:#e8f5e9,stroke:#388e3c
    style Visualize fill:#fce4ec,stroke:#c2185b
    style Output fill:#f3e5f5,stroke:#7b1fa2
    style Storage fill:#eceff1,stroke:#607d8b
```

#### ASCII Version

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           Data Flow Pipeline                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│   ┌───────────┐     ┌───────────┐     ┌───────────┐     ┌───────────────┐   │
│   │  DICOM    │     │   PACS    │     │   ITK     │     │     VTK       │   │
│   │  Files    │     │  Dataset  │     │   Image   │     │  ImageData    │   │
│   │           │     │           │     │           │     │               │   │
│   │  (*.dcm)  │ ──→ │ dicom_    │ ──→ │ itk::     │ ──→ │ vtkImageData  │   │
│   │           │     │ dataset   │     │ Image<>   │     │               │   │
│   └───────────┘     └───────────┘     └───────────┘     └───────────────┘   │
│        │                  │                  │                  │            │
│        │ pacs_system      │ PACSToITK       │ ITKVtkGlue       │ VTK        │
│        │ dicom_file::     │ Adapter         │ Connector        │ Mapper     │
│        │ open()           │                  │                  │            │
│        │                  │                  │                  ↓            │
│        │                  │                  │          ┌───────────────┐   │
│        │                  │                  │          │    Render     │   │
│        │                  │                  │          │    Output     │   │
│        │                  │                  │          │               │   │
│        │                  │                  │          │ • Volume      │   │
│        │                  │                  │          │ • Surface     │   │
│        │                  │                  │          │ • MPR         │   │
│        │                  │                  │          │ • 2D          │   │
│        │                  │                  │          └───────────────┘   │
│        │                  │                  │                               │
│        │                  │                  ↓                               │
│        │                  │          ┌───────────────┐                      │
│        │                  │          │  Processing   │                      │
│        │                  │          │               │                      │
│        │                  │          │ • Filtering   │                      │
│        │                  │          │ • Segmentation│                      │
│        │                  │          │ • Registration│                      │
│        │                  │          └───────────────┘                      │
│        │                  │                                                  │
│        ↓                  ↓                                                  │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                        Metadata Storage                              │   │
│   │                                                                       │   │
│   │   Patient Info  │  Study Info  │  Series Info  │  Image Info        │   │
│   │                                                                       │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

### SDS-ARCH-005: Remote Visualization Architecture (Alternative)

**Traces to**: REF-007, PRD Section 6

**Purpose**: Server-side rendering + image streaming architecture option for platform-independent medical image viewing

> **Note**: This architecture is an alternative approach that enables viewing medical images without GPU on various platforms such as web, iOS, Android, and desktop. For detailed implementation, see [REF-007: Remote Visualization](reference/07-remote-visualization.md).

#### Architecture Comparison

| Aspect | Desktop Architecture (Default) | Remote Visualization Architecture |
|--------|-------------------------------|-----------------------------------|
| **Rendering Location** | Client (Local GPU) | Server (Central GPU) |
| **Data Transfer** | DICOM file download | Image stream (JPEG/H.264) |
| **Client Requirements** | GPU, VTK library | Web browser or lightweight app |
| **Bandwidth Usage** | High (initial download) | Medium (continuous streaming) |
| **Security** | Local data storage | PHI retained on server (HIPAA-friendly) |
| **Scalability** | Independent per client | Shared GPU cluster |

#### Remote Visualization System Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    Remote Visualization Architecture                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│   Client Side (Any Platform)          Server Side (VTK Backend)              │
│   ─────────────────────────           ────────────────────────               │
│                                                                               │
│   ┌───────────────────────┐           ┌─────────────────────────────────┐   │
│   │  📱 Mobile App         │           │        VTK Render Server        │   │
│   │  🌐 Web Browser        │           │                                 │   │
│   │  💻 Desktop Thin Client│   WS      │  ┌─────────────────────────┐   │   │
│   │  📺 Smart TV           │ ◄────────►│  │  vtkRenderWindow        │   │   │
│   └───────────────────────┘  Events    │  │  (Offscreen Rendering)  │   │   │
│            │                           │  └─────────────────────────┘   │   │
│            │                           │              │                 │   │
│            │       Image Stream        │              ▼                 │   │
│            │ ◄─────────────────────────│  ┌─────────────────────────┐   │   │
│            │     (JPEG/H.264)          │  │  Image Encoder          │   │   │
│            │                           │  │  (JPEG/WebP/H.264)      │   │   │
│            ▼                           │  └─────────────────────────┘   │   │
│   ┌───────────────────────┐           │              │                 │   │
│   │  Display Layer        │           │              ▼                 │   │
│   │  • <canvas>           │           │  ┌─────────────────────────┐   │   │
│   │  • <video>            │           │  │  ITK + pacs_system      │   │   │
│   │  • Native View        │           │  │  (Processing Layer)     │   │   │
│   └───────────────────────┘           │  └─────────────────────────┘   │   │
│                                        └─────────────────────────────────┘   │
│                                                                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

#### Key Components

| Component | Responsibility | Technology |
|-----------|----------------|------------|
| **RenderServer** | Offscreen VTK rendering, camera manipulation | VTK, vtkRenderWindow |
| **StreamingServer** | WebSocket communication, session management, frame streaming | WebSocket++, nlohmann/json |
| **AdaptiveQualityManager** | Quality adjustment based on network conditions | Custom |
| **ImageEncoder** | JPEG/WebP/H.264 encoding | libjpeg, libwebp, NVENC |

#### When to Use Remote Visualization

| Use Case | Recommended Architecture |
|----------|-------------------------|
| High-performance workstation diagnosis | Desktop (Default) |
| Telemedicine / Collaboration | Remote Visualization |
| Mobile reference | Remote Visualization |
| Large datasets (>1GB) | Remote Visualization |
| Offline environment | Desktop (Default) |
| Web-based PACS integration | Remote Visualization |

#### Implementation Reference

For detailed implementation code, client examples (React, Flutter), and deployment configuration (Docker, Kubernetes), see the following documentation:

- **[REF-007: Remote Visualization Architecture](reference/07-remote-visualization.md)** - Complete architecture and implementation guide

---

## 3. Module Design

### SDS-MOD-001: Core Module (dicom_viewer_core)

**Traces to**: SRS-FR-001, SRS-FR-002, SRS-FR-003, SRS-FR-004

**Purpose**: Provide common data structures, utilities, and type definitions

**Components**:

| Component | Description | File Location |
|-----------|-------------|---------------|
| DicomLoader | DICOM file/series loading (GDCM) | `include/core/dicom_loader.hpp` |
| SeriesBuilder | Series assembly from slices | `include/core/series_builder.hpp` |
| ImageConverter | ITK-VTK image conversion | `include/core/image_converter.hpp` |
| HounsfieldConverter | HU value conversion | `include/core/hounsfield_converter.hpp` |
| TransferSyntaxDecoder | Transfer syntax support detection | `include/core/transfer_syntax_decoder.hpp` |
| Logging | Centralized logging (spdlog) | `include/core/logging.hpp` |

> **Note**: The original design specified `ImageBridge`, `MetadataStore`, and `CoordinateSystem` classes.
> In the current implementation, `ImageConverter` replaces `ImageBridge`, metadata is handled inline by `DicomLoader`,
> and coordinate conversion is provided by `MPRCoordinateTransformer` in the coordinate service module.

**Class Diagram**:

#### Mermaid Version

```mermaid
classDiagram
    class ImageTypes {
        <<typedef>>
        ITKImageType3D : itk::Image~short,3~
        ITKMaskType3D : itk::Image~uint8_t,3~
        ITKFloatType3D : itk::Image~float,3~
        VTKImagePtr : vtkSmartPointer~vtkImageData~
    }

    class ImageBridge {
        -m_itkToVtkConnector : ITKToVTKConnector
        -m_vtkToItkConnector : VTKToITKConnector
        +toVTK(itkImage) vtkImageData*
        +toITK(vtkImage) ITKImageType3D::Pointer
        +syncDirection()
        +preserveMetadata()
    }

    class MetadataStore {
        -m_patients : vector~Patient~
        -m_studies : vector~Study~
        -m_series : vector~Series~
        +addPatient(patient)
        +findStudies(patientId) vector~Study~
        +getMetadata(tag) string
        +clear()
    }

    class TransferFunctionPreset {
        +name : string
        +colorPoints : vector~ColorPoint~
        +opacityPoints : vector~OpacityPoint~
        +windowWidth : double
        +windowCenter : double
        +gradientOpacity : double
        +toVTKColorTF() vtkColorTransferFunction*
        +toVTKOpacityTF() vtkPiecewiseFunction*
    }

    class CoordinateConverter {
        +lpsToRas(point) Point3D
        +rasToLps(point) Point3D
        +imageToPhysical(index, origin, spacing, direction) Point3D
        +physicalToImage(point, origin, spacing, direction) Index3D
    }

    class ColorPoint {
        +hu : double
        +r : double
        +g : double
        +b : double
    }

    class OpacityPoint {
        +hu : double
        +opacity : double
    }

    ImageBridge ..> ImageTypes : uses
    TransferFunctionPreset *-- ColorPoint
    TransferFunctionPreset *-- OpacityPoint
```

#### ASCII Version

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        Core Module Class Diagram                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│   ┌───────────────────────────────────┐                                      │
│   │           ImageTypes              │                                      │
│   ├───────────────────────────────────┤                                      │
│   │ using ITKImageType3D =            │                                      │
│   │   itk::Image<short, 3>            │                                      │
│   │ using ITKMaskType3D =             │                                      │
│   │   itk::Image<uint8_t, 3>          │                                      │
│   │ using ITKFloatType3D =            │                                      │
│   │   itk::Image<float, 3>            │                                      │
│   └───────────────────────────────────┘                                      │
│                                                                               │
│   ┌───────────────────────────────────┐    ┌───────────────────────────────┐│
│   │         ImageBridge               │    │      MetadataStore            ││
│   ├───────────────────────────────────┤    ├───────────────────────────────┤│
│   │ + toVTK(itk::Image) : vtkImageData│    │ - m_patients : vector<Patient>││
│   │ + toITK(vtkImageData) : itk::Image│    │ - m_studies : vector<Study>   ││
│   │ + syncDirection()                 │    │ + addPatient()                ││
│   │ + preserveMetadata()              │    │ + findStudies()               ││
│   ├───────────────────────────────────┤    │ + getMetadata(tag)            ││
│   │ - m_itkToVtkConnector             │    └───────────────────────────────┘│
│   │ - m_vtkToItkConnector             │                                      │
│   └───────────────────────────────────┘                                      │
│                                                                               │
│   ┌───────────────────────────────────┐    ┌───────────────────────────────┐│
│   │     TransferFunctionPreset        │    │    CoordinateConverter        ││
│   ├───────────────────────────────────┤    ├───────────────────────────────┤│
│   │ + name : string                   │    │ + lpsToRas(point) : Point3D   ││
│   │ + colorPoints : vector<ColorPoint>│    │ + rasToLps(point) : Point3D   ││
│   │ + opacityPoints : vector<OpPoint> │    │ + imageToPhysical()           ││
│   │ + windowWidth : double            │    │ + physicalToImage()           ││
│   │ + windowCenter : double           │    └───────────────────────────────┘│
│   └───────────────────────────────────┘                                      │
│                                                                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

### SDS-MOD-002: Image Service Module

**Traces to**: SRS-FR-001 ~ SRS-FR-004, SRS-FR-016 ~ SRS-FR-025, SRS-FR-041, SRS-FR-042, SRS-FR-055

**Purpose**: Provide DICOM loading, preprocessing, segmentation (including advanced tools), and conversion functionality

**Components**:

> **Implementation Note**: The original design specified a unified `ImageService` facade with `IImageService` interface.
> The current implementation uses **direct component access** — each component is a standalone class without a facade orchestrator.

| Component | Description | Traces to |
|-----------|-------------|-----------|
| GaussianSmoother | Gaussian smoothing filter | SRS-FR-016 |
| AnisotropicDiffusionFilter | Edge-preserving noise reduction | SRS-FR-017 |
| N4BiasCorrector | MRI bias field correction | SRS-FR-018 |
| IsotropicResampler | Isotropic voxel resampling | SRS-FR-019 |
| HistogramEqualizer | Histogram equalization | SRS-FR-041 |
| ThresholdSegmenter | Otsu/manual threshold segmentation | SRS-FR-020 |
| RegionGrowingSegmenter | Seed-based region growing | SRS-FR-021 |
| LevelSetSegmenter | Geodesic active contour | SRS-FR-022 |
| WatershedSegmenter | Watershed transform | SRS-FR-042 |
| ManualSegmentationController | Brush, eraser, fill, smart scissors | SRS-FR-023 |
| MorphologicalProcessor | Erosion, dilation, opening, closing | SRS-FR-025 |
| LabelManager | Multi-label management and merging | SRS-FR-024 |
| CenterlineTracer | Vessel centerline extraction between seed points | SRS-FR-055 |
| LevelTracingTool | Edge-following contour at intensity boundary | SRS-FR-055 |
| HollowTool | Hollow shell creation with configurable wall thickness | SRS-FR-055 |
| MaskSmoother | Binary mask smoothing via morphological operations | SRS-FR-055 |
| SliceInterpolator | Morphological interpolation between annotated slices | SRS-FR-055 |
| MaskBooleanOperations | Union, intersection, difference, XOR on masks | SRS-FR-055 |
| SegmentationCommand | Command pattern for undo/redo segmentation actions | SRS-FR-055 |
| SnapshotCommand | Snapshot-based undo stack for segmentation state | SRS-FR-055 |
| PhaseTracker | Phase-aware segmentation tracking | SRS-FR-055 |
| EllipseROI | Elliptical region of interest tool | SRS-FR-055 |

**Class Diagram**:

#### Mermaid Version

```mermaid
classDiagram
    class IImageService {
        <<interface>>
        +loadSeries(path) Result~ImageData~
        +loadFile(path) Result~ImageData~
        +applyFilter(image, filter) ImageData
        +segment(image, params) MaskData
        +convertToHU(image) ImageData
    }

    class ImageService {
        -m_loader : unique_ptr~DicomLoader~
        -m_preprocessor : unique_ptr~Preprocessor~
        -m_segmentor : unique_ptr~Segmentor~
        -m_codecManager : unique_ptr~CodecManager~
        +loadSeries(path) Result~ImageData~
        +loadFile(path) Result~ImageData~
        +applyGaussian(sigma) ImageData
        +applyAnisotropic(iterations, conductance) ImageData
        +segmentThreshold(lower, upper) MaskData
        +segmentRegionGrow(seed, tolerance) MaskData
        +segmentLevelSet(params) MaskData
        +applyMorphology(operation, radius) MaskData
    }

    class DicomLoader {
        +loadFromDir(path) Result~SliceInfo[]~
        +loadFromFile(path) Result~ImageData~
        +sortSlices(slices) SliceInfo[]
        +buildVolume(slices) ITKImageType3D
    }

    class Preprocessor {
        +gaussian(image, sigma) ImageData
        +anisotropic(image, iter, cond) ImageData
        +histogramEqualization(image) ImageData
        +n4BiasCorrection(image) ImageData
        +resample(image, spacing) ImageData
    }

    class Segmentor {
        +threshold(image, lower, upper) MaskData
        +otsu(image) MaskData
        +regionGrow(image, seed, tol) MaskData
        +confidenceConnected(image, seed) MaskData
        +levelSet(image, params) MaskData
        +watershed(image) MaskData
        +morphology(mask, op, radius) MaskData
    }

    class CodecManager {
        +decode(data, transferSyntax) PixelData
        +isSupported(transferSyntax) bool
        +getCodec(transferSyntax) ICodec*
    }

    IImageService <|.. ImageService : implements
    ImageService o-- DicomLoader
    ImageService o-- Preprocessor
    ImageService o-- Segmentor
    DicomLoader o-- CodecManager
```

#### ASCII Version

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     Image Service Module Class Diagram                       │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                         IImageService <<interface>>                  │   │
│   ├─────────────────────────────────────────────────────────────────────┤   │
│   │ + loadSeries(path) : Result<ImageData>                              │   │
│   │ + loadFile(path) : Result<ImageData>                                │   │
│   │ + applyFilter(image, filter) : ImageData                            │   │
│   │ + segment(image, params) : MaskData                                 │   │
│   │ + convertToHU(image) : ImageData                                    │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                      △                                       │
│                                      │                                       │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                         ImageService                                 │   │
│   ├─────────────────────────────────────────────────────────────────────┤   │
│   │ - m_loader : unique_ptr<DicomLoader>                                │   │
│   │ - m_preprocessor : unique_ptr<Preprocessor>                         │   │
│   │ - m_segmentor : unique_ptr<Segmentor>                               │   │
│   │ - m_codecManager : unique_ptr<CodecManager>                         │   │
│   ├─────────────────────────────────────────────────────────────────────┤   │
│   │ + loadSeries(path) : Result<ImageData>                              │   │
│   │ + loadFile(path) : Result<ImageData>                                │   │
│   │ + applyGaussian(sigma) : ImageData                                  │   │
│   │ + applyAnisotropic(iterations, conductance) : ImageData             │   │
│   │ + segmentThreshold(lower, upper) : MaskData                         │   │
│   │ + segmentRegionGrow(seed, tolerance) : MaskData                     │   │
│   │ + segmentLevelSet(params) : MaskData                                │   │
│   │ + applyMorphology(operation, radius) : MaskData                     │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                      │                                       │
│              ┌───────────────────────┼───────────────────────┐              │
│              │                       │                       │              │
│              ↓                       ↓                       ↓              │
│   ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────────┐    │
│   │   DicomLoader   │    │   Preprocessor  │    │     Segmentor       │    │
│   ├─────────────────┤    ├─────────────────┤    ├─────────────────────┤    │
│   │ + loadFromDir() │    │ + gaussian()    │    │ + threshold()       │    │
│   │ + loadFromFile()│    │ + anisotropic() │    │ + otsu()            │    │
│   │ + sortSlices()  │    │ + histogramEq() │    │ + regionGrow()      │    │
│   │ + buildVolume() │    │ + n4BiasCorr()  │    │ + confidenceConn()  │    │
│   └─────────────────┘    │ + resample()    │    │ + levelSet()        │    │
│           │              └─────────────────┘    │ + watershed()       │    │
│           │                                     │ + morphology()      │    │
│           ↓                                     └─────────────────────┘    │
│   ┌─────────────────┐                                                       │
│   │  CodecManager   │                                                       │
│   ├─────────────────┤                                                       │
│   │ + decode(data,  │                                                       │
│   │   transferSyn.) │                                                       │
│   │ + isSupported() │                                                       │
│   │ + getCodec()    │                                                       │
│   └─────────────────┘                                                       │
│                                                                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

### SDS-MOD-003: Render Service Module

**Traces to**: SRS-FR-005 ~ SRS-FR-015

**Purpose**: Provide volume rendering, surface rendering, MPR, and 2D view functionality

**Components**:

| Component | Description | Traces to |
|-----------|-------------|-----------|
| VolumeRenderer | GPU volume ray casting (with CPU fallback) | SRS-FR-005, SRS-FR-006 |
| SurfaceRenderer | Marching Cubes isosurface extraction | SRS-FR-012 ~ SRS-FR-015 |
| MPRRenderer | Multiplanar reconstruction (axial/coronal/sagittal) | SRS-FR-008 ~ SRS-FR-011 |
| ObliquResliceRenderer | Arbitrary angle reslicing | SRS-FR-011 |
| TransferFunctionManager | Transfer function preset management | SRS-FR-006 |
| DRViewer | Dedicated DR/CR 2D viewer | SRS-FR-033 |
| HemodynamicOverlayRenderer | WSS/pressure overlay on volume rendering | SRS-FR-047 |
| StreamlineOverlayRenderer | Streamline tubes in volume viewer | SRS-FR-046 |
| HemodynamicSurfaceManager | Vessel surface hemodynamic mapping | SRS-FR-047 |
| ASCViewController | Multi-phase cardiac view control | SRS-FR-050 |

> **Implementation Note**: The class diagram below shows an `IRenderService` interface from the original design.
> This interface is **not implemented** — components are accessed directly. See SDS-IF-001 for details.

**Class Diagram**:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     Render Service Module Class Diagram                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                       IRenderService <<interface>>                   │   │
│   ├─────────────────────────────────────────────────────────────────────┤   │
│   │ + setImageData(data : vtkImageData)                                 │   │
│   │ + renderVolume(preset : TransferFunctionPreset)                     │   │
│   │ + renderSurface(threshold : double)                                 │   │
│   │ + renderMPR(orientation : MPROrientation)                           │   │
│   │ + render2D(slice : int)                                             │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                      △                                       │
│                                      │                                       │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                          RenderService                               │   │
│   ├─────────────────────────────────────────────────────────────────────┤   │
│   │ - m_volumeRenderer : unique_ptr<VolumeRenderer>                     │   │
│   │ - m_surfaceRenderer : unique_ptr<SurfaceRenderer>                   │   │
│   │ - m_mprRenderer : unique_ptr<MPRRenderer>                           │   │
│   │ - m_sliceViewer : unique_ptr<SliceViewer>                           │   │
│   │ - m_tfManager : unique_ptr<TransferFunctionManager>                 │   │
│   ├─────────────────────────────────────────────────────────────────────┤   │
│   │ + setImageData(data)                                                │   │
│   │ + setMaskData(mask)                                                 │   │
│   │ + setRenderWindow(window)                                           │   │
│   │ + renderVolume(preset)                                              │   │
│   │ + renderSurface(threshold, color, opacity)                          │   │
│   │ + renderMultiSurface(surfaces)                                      │   │
│   │ + renderMPR(orientation, slice)                                     │   │
│   │ + setWindowLevel(window, level)                                     │   │
│   │ + setClippingBox(bounds)                                            │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                      │                                       │
│         ┌────────────────────────────┼────────────────────────────┐         │
│         │                            │                            │         │
│         ↓                            ↓                            ↓         │
│   ┌───────────────┐       ┌────────────────────┐      ┌─────────────────┐  │
│   │VolumeRenderer │       │  SurfaceRenderer   │      │   MPRRenderer   │  │
│   ├───────────────┤       ├────────────────────┤      ├─────────────────┤  │
│   │- m_mapper     │       │- m_marchingCubes   │      │- m_reslice[3]   │  │
│   │- m_volume     │       │- m_smoother        │      │- m_viewer[3]    │  │
│   │- m_property   │       │- m_decimator       │      │- m_crosshair    │  │
│   │- m_colorTF    │       │- m_actors[]        │      ├─────────────────┤  │
│   │- m_opacityTF  │       ├────────────────────┤      │+ setSlice()     │  │
│   ├───────────────┤       │+ extract(thresh)   │      │+ setOrientation │  │
│   │+ render()     │       │+ smooth(iter)      │      │+ syncCrosshair()│  │
│   │+ setPreset()  │       │+ decimate(ratio)   │      │+ setWindowLevel │  │
│   │+ setClipBox() │       │+ setColor(color)   │      │+ getThickSlab() │  │
│   │+ setMIP()     │       │+ exportSTL(path)   │      └─────────────────┘  │
│   └───────────────┘       │+ exportPLY(path)   │                           │
│                           └────────────────────┘                           │
│                                                                               │
│   ┌───────────────────────────────────────────────────────────────────┐     │
│   │              TransferFunctionManager                               │     │
│   ├───────────────────────────────────────────────────────────────────┤     │
│   │ - m_presets : map<string, TransferFunctionPreset>                 │     │
│   │ - m_currentPreset : string                                        │     │
│   ├───────────────────────────────────────────────────────────────────┤     │
│   │ + loadPresets()                                                   │     │
│   │ + getPreset(name) : TransferFunctionPreset                        │     │
│   │ + saveCustomPreset(name, preset)                                  │     │
│   │ + applyPreset(name, volumeProperty)                               │     │
│   │ + getPresetNames() : vector<string>                               │     │
│   └───────────────────────────────────────────────────────────────────┘     │
│                                                                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

### SDS-MOD-004: Measurement Service Module

**Traces to**: SRS-FR-026 ~ SRS-FR-031

**Purpose**: Distance, angle, area, volume measurement and ROI management

**Components**:

| Component | Description | Traces to |
|-----------|-------------|-----------|
| LinearMeasurementTool | Distance, angle, Cobb angle measurement | SRS-FR-026 |
| AreaMeasurementTool | Ellipse, rectangle, polygon, freehand ROI | SRS-FR-027 |
| VolumeCalculator | 3D volume calculation from segmented regions | SRS-FR-029 |
| ROIStatistics | Mean, StdDev, Min/Max, histogram for ROI | SRS-FR-028 |
| ShapeAnalyzer | Sphericity, elongation, principal axes | SRS-FR-030 |
| MPRCoordinateTransformer | World/screen/image coordinate conversion | SRS-FR-008 |

> **Implementation Note**: The class diagram below shows an `IMeasurementService` interface from the original design.
> This interface is **not implemented** — components are accessed directly. See SDS-IF-001 for details.

**Class Diagram**:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                   Measurement Service Module Class Diagram                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                   IMeasurementService <<interface>>                  │   │
│   ├─────────────────────────────────────────────────────────────────────┤   │
│   │ + measureDistance(p1, p2) : double                                  │   │
│   │ + measureAngle(p1, p2, p3) : double                                 │   │
│   │ + measureArea(roi) : double                                         │   │
│   │ + measureVolume(mask) : double                                      │   │
│   │ + calculateStatistics(image, mask) : Statistics                     │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                      △                                       │
│                                      │                                       │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                       MeasurementService                             │   │
│   ├─────────────────────────────────────────────────────────────────────┤   │
│   │ - m_linearMeasure : unique_ptr<LinearMeasurement>                   │   │
│   │ - m_areaMeasure : unique_ptr<AreaMeasurement>                       │   │
│   │ - m_volumeMeasure : unique_ptr<VolumeMeasurement>                   │   │
│   │ - m_statsCalc : unique_ptr<StatisticsCalculator>                    │   │
│   │ - m_roiManager : unique_ptr<ROIManager>                             │   │
│   ├─────────────────────────────────────────────────────────────────────┤   │
│   │ + measureDistance(p1, p2) : MeasureResult                           │   │
│   │ + measureAngle(p1, p2, p3) : MeasureResult                          │   │
│   │ + measureCobbAngle(lines) : MeasureResult                           │   │
│   │ + createEllipseROI(center, axes) : ROI                              │   │
│   │ + createPolygonROI(points) : ROI                                    │   │
│   │ + createFreehandROI(points) : ROI                                   │   │
│   │ + calculateROIArea(roi) : double                                    │   │
│   │ + calculateROIPerimeter(roi) : double                               │   │
│   │ + calculateMaskVolume(mask) : VolumeResult                          │   │
│   │ + calculateMaskSurfaceArea(mask) : double                           │   │
│   │ + calculateROIStatistics(image, roi) : Statistics                   │   │
│   │ + calculateHistogram(image, roi) : Histogram                        │   │
│   │ + generateReport() : AnalysisReport                                 │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                               │
│   ┌─────────────────────┐    ┌─────────────────────┐                        │
│   │  LinearMeasurement  │    │   AreaMeasurement   │                        │
│   ├─────────────────────┤    ├─────────────────────┤                        │
│   │ + distance(p1, p2)  │    │ + ellipseArea()     │                        │
│   │ + angle(p1, p2, p3) │    │ + polygonArea()     │                        │
│   │ + cobbAngle()       │    │ + freehandArea()    │                        │
│   │ + multiDistance()   │    │ + perimeter()       │                        │
│   └─────────────────────┘    └─────────────────────┘                        │
│                                                                               │
│   ┌─────────────────────┐    ┌─────────────────────┐                        │
│   │  VolumeMeasurement  │    │StatisticsCalculator │                        │
│   ├─────────────────────┤    ├─────────────────────┤                        │
│   │ + voxelCount()      │    │ + mean()            │                        │
│   │ + volumeMm3()       │    │ + stdDev()          │                        │
│   │ + volumeCm3()       │    │ + min() / max()     │                        │
│   │ + surfaceArea()     │    │ + median()          │                        │
│   │ + boundingBox()     │    │ + histogram()       │                        │
│   │ + centroid()        │    │ + percentiles()     │                        │
│   │ + sphericity()      │    └─────────────────────┘                        │
│   │ + elongation()      │                                                    │
│   └─────────────────────┘                                                    │
│                                                                               │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                          ROIManager                                  │   │
│   ├─────────────────────────────────────────────────────────────────────┤   │
│   │ - m_rois : vector<unique_ptr<ROI>>                                  │   │
│   │ - m_selectedIndex : int                                             │   │
│   ├─────────────────────────────────────────────────────────────────────┤   │
│   │ + addROI(roi) : int                                                 │   │
│   │ + removeROI(index)                                                  │   │
│   │ + getROI(index) : ROI&                                              │   │
│   │ + selectROI(index)                                                  │   │
│   │ + setROIName(index, name)                                           │   │
│   │ + setROIColor(index, color)                                         │   │
│   │ + setROIVisible(index, visible)                                     │   │
│   │ + copyROI(srcIndex, destSlice)                                      │   │
│   │ + saveROIs(path)                                                    │   │
│   │ + loadROIs(path)                                                    │   │
│   │ + getAllROIs() : vector<ROI>                                        │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

### SDS-MOD-005: Network Service Module

**Traces to**: SRS-FR-034 ~ SRS-FR-038

**Purpose**: PACS integration (C-FIND, C-MOVE, C-STORE, C-ECHO)

**Components**:

| Component | Description | Traces to |
|-----------|-------------|-----------|
| DicomFindSCU | C-FIND query (Patient/Study/Series/Image level) | SRS-FR-035 |
| DicomMoveSCU | C-MOVE retrieval with pending status handling | SRS-FR-036 |
| DicomStoreSCP | C-STORE SCP receive server | SRS-FR-037 |
| DicomEchoSCU | C-ECHO connectivity verification | SRS-FR-034 |
| PacsConfigManager | PACS server configuration management | SRS-FR-038 |

> **Note**: All PACS components use the `pacs_system` library (pacs::services, pacs::network, pacs::core).
> The original design specified `QueryClient`, `RetrieveClient`, etc. — these were renamed during the
> DCMTK → pacs_system migration (#110-#117) to follow DICOM service class naming conventions.

**Class Diagram**:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    Network Service Module Class Diagram                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                    INetworkService <<interface>>                     │   │
│   ├─────────────────────────────────────────────────────────────────────┤   │
│   │ + echo(server) : bool                                               │   │
│   │ + find(server, query) : vector<DicomDataset>                        │   │
│   │ + move(server, query, destAE) : bool                                │   │
│   │ + startStorageSCP(config) : bool                                    │   │
│   │ + stopStorageSCP()                                                  │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                      △                                       │
│                                      │                                       │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                         NetworkService                               │   │
│   ├─────────────────────────────────────────────────────────────────────┤   │
│   │ - m_echoClient : unique_ptr<EchoClient>                             │   │
│   │ - m_queryClient : unique_ptr<QueryClient>                           │   │
│   │ - m_retrieveClient : unique_ptr<RetrieveClient>                     │   │
│   │ - m_storageServer : unique_ptr<StorageServer>                       │   │
│   │ - m_configManager : unique_ptr<PACSConfigManager>                   │   │
│   ├─────────────────────────────────────────────────────────────────────┤   │
│   │ + verifyConnection(server) : bool                                   │   │
│   │ + queryPatients(server, criteria) : vector<PatientInfo>             │   │
│   │ + queryStudies(server, patientId) : vector<StudyInfo>               │   │
│   │ + querySeries(server, studyUid) : vector<SeriesInfo>                │   │
│   │ + retrieveStudy(server, studyUid, destDir) : bool                   │   │
│   │ + retrieveSeries(server, seriesUid, destDir) : bool                 │   │
│   │ + startReceiver(port, storageDir) : bool                            │   │
│   │ + stopReceiver()                                                    │   │
│   │ + getServerList() : vector<PACSServerConfig>                        │   │
│   │ + addServer(config)                                                 │   │
│   │ + removeServer(name)                                                │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                               │
│   ┌─────────────────────┐  ┌─────────────────────┐  ┌─────────────────────┐│
│   │    QueryClient      │  │   RetrieveClient    │  │   StorageServer     ││
│   ├─────────────────────┤  ├─────────────────────┤  ├─────────────────────┤│
│   │ + findPatient()     │  │ + moveStudy()       │  │ + start()           ││
│   │ + findStudy()       │  │ + moveSeries()      │  │ + stop()            ││
│   │ + findSeries()      │  │ + moveImage()       │  │ + onImageReceived() ││
│   │ + findImage()       │  │ + setDestAE()       │  │ + setStoragePath()  ││
│   └─────────────────────┘  └─────────────────────┘  └─────────────────────┘│
│                                                                               │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                       PACSServerConfig                               │   │
│   ├─────────────────────────────────────────────────────────────────────┤   │
│   │ + name : string                                                     │   │
│   │ + aeTitle : string                                                  │   │
│   │ + host : string                                                     │   │
│   │ + port : uint16_t                                                   │   │
│   │ + useTLS : bool                                                     │   │
│   │ + timeout : int                                                     │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

### SDS-MOD-006: UI Module

**Traces to**: SRS-FR-039, SRS-FR-040, SRS-FR-056

**Purpose**: Provide Qt6-based user interface with comprehensive panels, dialogs, and widgets

**Components**:

| Component | Description | Traces to | Status |
|-----------|-------------|-----------|--------|
| MainWindow | Main window with dockable panels, dark theme | SRS-FR-039 | ✅ Implemented |
| ViewportWidget | VTK rendering widget with QVTKOpenGLNativeWidget | SRS-FR-039 | ✅ Implemented |
| PatientBrowser | Patient/study/series tree browser with search | SRS-FR-039 | ✅ Implemented |
| ToolsPanel | Window/level controls, presets, visualization modes | SRS-FR-039 | ✅ Implemented |
| SegmentationPanel | Segmentation tools panel (brush, eraser, fill, polygon, smart scissors) | SRS-FR-024 | ✅ Implemented |
| StatisticsPanel | ROI statistics display, histogram, multi-ROI comparison, CSV export | SRS-FR-028 | ✅ Implemented |
| OverlayControlPanel | Overlay visibility and parameter controls | SRS-FR-039 | ✅ Implemented |
| FlowToolPanel | 4D Flow analysis tool controls (streamlines, planes, quantification) | SRS-FR-046 | ✅ Implemented |
| WorkflowPanel | Workflow step management panel | SRS-FR-039 | ✅ Implemented |
| ReportPanel | Report generation and preview panel | SRS-FR-054 | ✅ Implemented |
| SettingsDialog | Application settings dialog (rendering, memory, paths) | SRS-FR-040 | ✅ Implemented |
| PacsConfigDialog | PACS server configuration and connection test | SRS-FR-038 | ✅ Implemented |
| QuantificationWindow | Flow quantification results window with contour editing | SRS-FR-047 | ✅ Implemented |
| MaskWizard | Step-by-step mask creation wizard | SRS-FR-055 | ✅ Implemented |
| VideoExportDialog | Video export configuration (format, FPS, codec) | SRS-FR-054 | ✅ Implemented |
| PhaseSliderWidget | Cardiac/temporal phase slider widget | SRS-FR-048 | ✅ Implemented |
| SPModeToggle | Single-phase / multi-phase mode toggle | SRS-FR-050 | ✅ Implemented |
| FlowGraphWidget | Time-velocity curve and flow rate graph display | SRS-FR-047 | ✅ Implemented |
| WorkflowTabBar | Workflow tab navigation bar | SRS-FR-039 | ✅ Implemented |
| MPRViewWidget | Dedicated MPR view widget with crosshair sync | SRS-FR-008 | ✅ Implemented |
| ViewportLayoutManager | Multi-viewport layout management (1x1, 2x2, 1x3) | SRS-FR-039 | ✅ Implemented |
| Display3DController | 3D display parameter control (lighting, clipping, orientation) | SRS-FR-005 | ✅ Implemented |
| DropHandler | Drag-and-drop DICOM and project file import handler | SRS-FR-039 | ✅ Implemented |
| IntroPage | Application intro/welcome page | SRS-FR-039 | ✅ Implemented |
| MaskWizardController | Controller for mask wizard workflow logic | SRS-FR-055 | ✅ Implemented |

**Widget Hierarchy**:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          UI Widget Hierarchy                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│   MainWindow (QMainWindow)                                                   │
│   │                                                                          │
│   ├── MenuBar (QMenuBar)                                                     │
│   │   ├── File Menu (Open, Save, Export, Close, Exit)                        │
│   │   ├── Edit Menu (Undo, Redo, Preferences)                                │
│   │   ├── View Menu (Layout, Presets, Window/Level)                          │
│   │   ├── Tools Menu (Measure, Segment, Annotations)                         │
│   │   ├── PACS Menu (Query, Retrieve, Send)                                  │
│   │   └── Help Menu (About, Manual)                                          │
│   │                                                                          │
│   ├── ToolBar (QToolBar)                                                     │
│   │   ├── Open Button                                                        │
│   │   ├── Save Button                                                        │
│   │   ├── PACS Button                                                        │
│   │   ├── Separator                                                          │
│   │   ├── Scroll Tool                                                        │
│   │   ├── Zoom Tool                                                          │
│   │   ├── Pan Tool                                                           │
│   │   ├── Window/Level Tool                                                  │
│   │   ├── Separator                                                          │
│   │   ├── Distance Tool                                                      │
│   │   ├── Angle Tool                                                         │
│   │   ├── ROI Tool                                                           │
│   │   ├── Separator                                                          │
│   │   └── Segmentation Tool                                                  │
│   │                                                                          │
│   ├── Central Widget (QSplitter)                                             │
│   │   │                                                                      │
│   │   ├── Left Dock: PatientBrowser (QDockWidget)                            │
│   │   │   ├── Patient Tree (QTreeView)                                       │
│   │   │   │   ├── Patient Node                                               │
│   │   │   │   │   ├── Study Node                                             │
│   │   │   │   │   │   └── Series Node                                        │
│   │   │   │   │   │       └── Image Count                                    │
│   │   │   └── Series Thumbnail (QListView)                                   │
│   │   │                                                                      │
│   │   ├── Center: ViewportContainer                                          │
│   │   │   │                                                                  │
│   │   │   ├── Layout 1x1: Single Viewport                                    │
│   │   │   │   └── QVTKOpenGLNativeWidget                                     │
│   │   │   │                                                                  │
│   │   │   ├── Layout 2x2: MPR + 3D                                           │
│   │   │   │   ├── Axial View (Top-Left)                                      │
│   │   │   │   ├── Coronal View (Top-Right)                                   │
│   │   │   │   ├── Sagittal View (Bottom-Left)                                │
│   │   │   │   └── 3D View (Bottom-Right)                                     │
│   │   │   │                                                                  │
│   │   │   └── Layout 1x3: MPR Only                                           │
│   │   │       ├── Axial View                                                 │
│   │   │       ├── Coronal View                                               │
│   │   │       └── Sagittal View                                              │
│   │   │                                                                      │
│   │   └── Right Dock: ToolsPanel (QDockWidget)                               │
│   │       │                                                                  │
│   │       ├── Window/Level Panel (QGroupBox)                                 │
│   │       │   ├── Window Slider                                              │
│   │       │   ├── Level Slider                                               │
│   │       │   └── Preset ComboBox                                            │
│   │       │                                                                  │
│   │       ├── Volume Rendering Panel (QGroupBox)                             │
│   │       │   ├── Preset ComboBox                                            │
│   │       │   ├── Transfer Function Editor                                   │
│   │       │   └── Shading Controls                                           │
│   │       │                                                                  │
│   │       ├── Segmentation Panel (QGroupBox)                                 │
│   │       │   ├── Algorithm ComboBox                                         │
│   │       │   ├── Parameters                                                 │
│   │       │   ├── Brush/Eraser Tools                                         │
│   │       │   ├── Morphology Tools                                           │
│   │       │   └── Label Manager                                              │
│   │       │                                                                  │
│   │       ├── Measurement Panel (QGroupBox)                                  │
│   │       │   ├── Tool Selection                                             │
│   │       │   ├── ROI List                                                   │
│   │       │   └── Statistics Display                                         │
│   │       │                                                                  │
│   │       └── ROI Management Panel (QGroupBox)                               │
│   │           ├── ROI List (QListWidget)                                     │
│   │           ├── Add/Remove Buttons                                         │
│   │           └── Properties Editor                                          │
│   │                                                                          │
│   └── StatusBar (QStatusBar)                                                 │
│       ├── Patient Info Label                                                 │
│       ├── Series Info Label                                                  │
│       ├── Slice Position Label                                               │
│       ├── Cursor Position Label (X, Y, Z)                                    │
│       ├── Pixel Value Label (HU / Signal)                                    │
│       └── Memory Usage Label                                                 │
│                                                                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

### SDS-MOD-007: Flow Analysis Module

**Traces to**: SRS-FR-043 ~ SRS-FR-048

**Purpose**: Provide 4D Flow MRI DICOM parsing, velocity field assembly, phase correction, flow visualization, hemodynamic quantification, and temporal navigation

**Components**:

> **Implementation Note**: This module is implemented as a service layer (`flow_service`) with dependencies on `image_service` (for DICOM loading) and `render_service` (for VTK integration). All components were implemented in Phase 4 (v0.6.0).

| Component | Description | Traces to | Status |
|-----------|-------------|-----------|--------|
| FlowDicomParser | Vendor-specific 4D Flow DICOM parsing (Siemens, Philips, GE) | SRS-FR-043 | ✅ Implemented |
| VelocityFieldAssembler | Vector field construction from velocity-encoded components with VENC scaling | SRS-FR-044 | ✅ Implemented |
| PhaseCorrector | Velocity aliasing unwrap, eddy current correction, Maxwell term correction | SRS-FR-045 | ✅ Implemented |
| FlowVisualizer | Streamline, pathline, and vector glyph rendering via VTK | SRS-FR-046 | ✅ Implemented |
| FlowQuantifier | Flow rate, time-velocity curves, pressure gradient calculations | SRS-FR-047 | ✅ Implemented |
| VesselAnalyzer | WSS, OSI, TKE, vorticity, and helicity analysis | SRS-FR-047 | ✅ Implemented |
| TemporalNavigator | Cardiac phase navigation, cine playback, sliding window cache | SRS-FR-048 | ✅ Implemented |

**Class Diagram**:

#### Mermaid Version

```mermaid
classDiagram
    class IFlowDicomParser {
        <<interface>>
        +detectVendor(dataset) VendorType
        +parseFlowSeries(path) Result~FlowSeriesInfo~
        +extractVENC(dataset) float
        +classifyComponent(dataset) VelocityComponent
    }

    class FlowDicomParser {
        -m_vendorParsers : map~VendorType, unique_ptr~IVendorFlowParser~~
        +detectVendor(dataset) VendorType
        +parseFlowSeries(path) Result~FlowSeriesInfo~
        +extractVENC(dataset) float
        +classifyComponent(dataset) VelocityComponent
        -selectParser(vendor) IVendorFlowParser*
    }

    class IVendorFlowParser {
        <<interface>>
        +parseVelocityTag(dataset) float
        +parseVENCTag(dataset) float
        +getExpectedIODType() string
    }

    class SiemensFlowParser {
        +parseVelocityTag(dataset) float
        +parseVENCTag(dataset) float
        +getExpectedIODType() string
    }

    class PhilipsFlowParser {
        +parseVelocityTag(dataset) float
        +parseVENCTag(dataset) float
        +getExpectedIODType() string
    }

    class GEFlowParser {
        +parseVelocityTag(dataset) float
        +parseVENCTag(dataset) float
        +getExpectedIODType() string
    }

    class VelocityFieldAssembler {
        -m_vencValues : array~float, 3~
        -m_bitsStored : int
        +assemble(flowInfo, components) Result~VelocityPhase~
        +applyVENCScaling(image, venc, isSigned) VectorImage
        -composeVectorField(vx, vy, vz) VectorImage
    }

    class PhaseCorrector {
        -m_unwrapThreshold : float
        -m_eddyCurrentOrder : int
        +correctAll(velocityPhase) VelocityPhase
        +unwrapAliasing(field, venc) VectorImage
        +correctEddyCurrent(field, magnitude) VectorImage
        +correctMaxwellTerms(field, gradientInfo) VectorImage
        -fitPolynomial(mask, velocity, order) PolynomialCoeffs
    }

    class FlowVisualizer {
        -m_streamTracer : vtkSmartPointer~vtkStreamTracer~
        -m_tubeFilter : vtkSmartPointer~vtkTubeFilter~
        -m_glyphFilter : vtkSmartPointer~vtkGlyph3D~
        -m_colorMode : ColorMappingMode
        +renderStreamlines(field, seeds) vtkActor*
        +renderPathlines(phases, seeds) vtkActor*
        +renderVectorGlyphs(field, skipFactor) vtkActor*
        +setColorMapping(mode) void
        -createSeedPoints(geometry) vtkPointSource*
    }

    class FlowQuantifier {
        +computeFlowRate(field, plane, contour) FlowMeasurement
        +computeTimeVelocityCurve(phases, plane) TimeVelocityCurve
        +computePressureGradient(field) float
        -extractThroughPlaneVelocity(field, plane) vector~float~
        -integrateSurface(velocities, areas) float
    }

    class VesselAnalyzer {
        -m_viscosity : float
        +computeWSS(phases, vesselMesh) WSSResult
        +computeOSI(wssTimeSeries) Image~float~
        +computeTKE(phases) Image~float~
        +computeVorticity(field) VectorImage
        -sampleNearWallVelocity(field, mesh) vector~Vec3~
    }

    class TemporalNavigator {
        -m_phaseCache : unique_ptr~PhaseCache~
        -m_currentPhase : int
        -m_playbackTimer : QTimer*
        -m_playbackSpeed : float
        +setPhase(index) void
        +play() void
        +pause() void
        +stop() void
        +setFrameRate(fps) void
        -prefetchAdjacentPhases() void
    }

    class PhaseCache {
        -m_windowSize : int
        -m_cache : map~int, VelocityPhase~
        -m_memoryBudget : size_t
        +get(phaseIndex) optional~VelocityPhase~
        +put(phaseIndex, phase) void
        +prefetch(indices) future~void~
        -evictLRU() void
    }

    IFlowDicomParser <|.. FlowDicomParser
    IVendorFlowParser <|.. SiemensFlowParser
    IVendorFlowParser <|.. PhilipsFlowParser
    IVendorFlowParser <|.. GEFlowParser
    FlowDicomParser --> IVendorFlowParser : uses
    FlowDicomParser ..> VelocityFieldAssembler : feeds
    VelocityFieldAssembler ..> PhaseCorrector : feeds
    PhaseCorrector ..> FlowVisualizer : feeds
    PhaseCorrector ..> FlowQuantifier : feeds
    PhaseCorrector ..> VesselAnalyzer : feeds
    TemporalNavigator --> PhaseCache : manages
    TemporalNavigator ..> FlowVisualizer : triggers update
```

#### ASCII Version

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    SDS-MOD-007: Flow Analysis Module                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│   ┌──────────────────────────────────────────────────────────────────┐      │
│   │                     FlowDicomParser                                │      │
│   │  ┌───────────────┬───────────────┬───────────────┐               │      │
│   │  │SiemensParser  │PhilipsParser  │GEParser       │  (Strategy)   │      │
│   │  │(0051,1014)    │(2005,1071)    │(0019,10cc)    │               │      │
│   │  └───────────────┴───────────────┴───────────────┘               │      │
│   └───────────────────────────┬──────────────────────────────────────┘      │
│                               ↓                                              │
│   ┌───────────────────────────────────────────────────────────────────┐     │
│   │                  VelocityFieldAssembler                             │     │
│   │  ITK: ComposeImageFilter → VectorImage<float,3>                    │     │
│   │  VENC Scaling: velocity = (pixel / max) × VENC                     │     │
│   └───────────────────────────┬─────────────────────────────────────┘      │
│                               ↓                                              │
│   ┌───────────────────────────────────────────────────────────────────┐     │
│   │                     PhaseCorrector                                  │     │
│   │  1. Aliasing Unwrap (Laplacian 3D)                                 │     │
│   │  2. Eddy Current (2nd-order polynomial fit)                        │     │
│   │  3. Maxwell Terms (concomitant gradient)                           │     │
│   └──────────┬────────────────┬────────────────┬────────────────────┘      │
│              ↓                ↓                ↓                             │
│   ┌──────────────┐  ┌────────────────┐  ┌─────────────────┐               │
│   │FlowVisualizer│  │FlowQuantifier  │  │VesselAnalyzer   │               │
│   │              │  │                │  │                 │               │
│   │• Streamlines │  │• Flow Rate     │  │• WSS / TAWSS    │               │
│   │• Pathlines   │  │• TVC           │  │• OSI            │               │
│   │• Glyphs      │  │• Pressure ΔP   │  │• TKE            │               │
│   │• Color Maps  │  │• SV / RF       │  │• Vorticity      │               │
│   └──────────────┘  └────────────────┘  └─────────────────┘               │
│                                                                               │
│   ┌───────────────────────────────────────────────────────────────────┐     │
│   │                   TemporalNavigator                                 │     │
│   │  ┌──────────────────────────────┐                                  │     │
│   │  │   PhaseCache                  │  Sliding Window: ±2 phases      │     │
│   │  │   LRU Eviction, Prefetch     │  Budget: ~250 MB (5 phases)     │     │
│   │  └──────────────────────────────┘                                  │     │
│   │  Cine: play/pause/stop, 1-30 fps, 0.5x-4x speed                  │     │
│   └───────────────────────────────────────────────────────────────────┘     │
│                                                                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Key Design Decisions**:

| Decision | Choice | Rationale |
|----------|--------|-----------|
| ITK Vector Type | `itk::VectorImage<float, 3>` | Interleaved storage, memory-efficient for 3-component velocity |
| Vendor Abstraction | Strategy Pattern (IVendorFlowParser) | Extensible for new vendors without modifying parser core |
| Memory Management | Sliding Window Cache (±2 phases) | Balances memory usage (~250 MB) with navigation responsiveness |
| VTK Integration | Direct pipeline (StreamTracer→TubeFilter→Mapper) | Standard VTK pipeline for maintainability and performance |
| Phase Correction Order | Aliasing → Eddy Current → Maxwell | Each stage depends on the previous correction being applied |

---

### SDS-MOD-008: Enhanced DICOM Module

**Traces to**: SRS-FR-049

**Purpose**: Parse Enhanced (multi-frame) DICOM IODs and extract frames with per-frame metadata, enabling compatibility with modern CT/MR scanners.

**Components**:

| Component | Description | Traces to | Status |
|-----------|-------------|-----------|--------|
| EnhancedDicomParser | Detect and parse Enhanced CT/MR IODs | SRS-FR-049 | ✅ Implemented |
| FrameExtractor | Extract individual frames from multi-frame pixel data | SRS-FR-049 | ✅ Implemented |
| FunctionalGroupParser | Parse Shared/PerFrame FunctionalGroupsSequence | SRS-FR-049 | ✅ Implemented |
| DimensionIndexSorter | Sort frames by DimensionIndexSequence | SRS-FR-049 | ✅ Implemented |
| SeriesClassifier | Classify Enhanced series by type | SRS-FR-049 | ✅ Implemented |

**Class Diagram**:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    SDS-MOD-008: Enhanced DICOM Module                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│   ┌───────────────────────────────────────────────────────────────────┐     │
│   │                    EnhancedDicomParser                              │     │
│   │  • detectEnhancedIOD(sopClassUID) → bool                          │     │
│   │  • parseMultiFrame(path) → Result<EnhancedSeriesInfo>             │     │
│   │  • assembleVolume(frames) → itk::Image<short,3>                   │     │
│   └───────────────────────────┬─────────────────────────────────────┘      │
│                               ↓                                              │
│   ┌──────────────────────────────────────────────────────────────────┐      │
│   │                    FunctionalGroupParser                           │      │
│   │  ┌────────────────────────┐  ┌─────────────────────────────────┐│      │
│   │  │ SharedFunctionalGroup  │  │ PerFrameFunctionalGroup         ││      │
│   │  │ (5200,9229)            │  │ (5200,9230)                     ││      │
│   │  │ • PixelValueTransform  │  │ • PlanePositionSequence         ││      │
│   │  │ • FrameContentSequence │  │ • PlaneOrientationSequence      ││      │
│   │  │ • PixelMeasures        │  │ • FrameContentSequence          ││      │
│   │  └────────────────────────┘  └─────────────────────────────────┘│      │
│   └──────────────────────────────────────────────────────────────────┘      │
│                               ↓                                              │
│   ┌──────────────────────────────────────────────────────────────────┐      │
│   │                    DimensionIndexSorter                            │      │
│   │  • parseDimensionIndex(0020,9222) → DimensionOrganization        │      │
│   │  • sortFrames(frames, dimIndex) → vector<FrameInfo>              │      │
│   │  • groupByDimension(frames, dimId) → map<int, vector<FrameInfo>> │      │
│   └──────────────────────────────────────────────────────────────────┘      │
│                                                                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Key Data Structures**:

```cpp
struct EnhancedFrameInfo {
    int frameIndex;
    std::array<double, 3> imagePosition;
    std::array<double, 6> imageOrientation;
    double sliceThickness;
    double rescaleSlope;
    double rescaleIntercept;
    std::optional<double> triggerTime;
    std::optional<int> temporalPositionIndex;
    std::map<uint32_t, int> dimensionIndices;  // dimension pointer → index value
};

struct EnhancedSeriesInfo {
    std::string sopClassUID;
    int numberOfFrames;
    int rows, columns;
    int bitsAllocated, bitsStored;
    std::vector<EnhancedFrameInfo> frames;
    // Shared metadata (common to all frames)
    double pixelSpacingX, pixelSpacingY;
};
```

**Key Design Decisions**:

| Decision | Choice | Rationale |
|----------|--------|-----------|
| IOD Detection | SOP Class UID lookup | Deterministic, no ambiguity |
| Frame Memory | On-demand extraction | Large multi-frame files may exceed memory if all loaded at once |
| Classic Fallback | Auto-routing by SOP Class | Seamless backward compatibility with existing loader |
| Pixel Data | Offset table + frame-by-frame | Required for encapsulated transfer syntaxes |

---

### SDS-MOD-009: Cardiac CT Analysis Module

**Traces to**: SRS-FR-050 ~ SRS-FR-053

**Purpose**: Provide ECG-gated cardiac CT phase separation, coronary CTA analysis, calcium scoring, and cine MRI temporal display.

**Components**:

| Component | Description | Traces to | Status |
|-----------|-------------|-----------|--------|
| CardiacPhaseDetector | Detect and separate ECG-gated cardiac phases | SRS-FR-050 | ✅ Implemented |
| CoronaryLineCenterlineExtractor | Extract coronary artery centerlines (Frangi vesselness + minimal path) | SRS-FR-051 | ✅ Implemented |
| CurvedPlanarReformatter | Generate CPR views along extracted centerlines | SRS-FR-051 | ✅ Implemented |
| CalciumScorer | Compute Agatston, volume, and mass calcium scores | SRS-FR-052 | ✅ Implemented |
| CineOrganizer | Detect and organize multi-phase cine MRI series | SRS-FR-053 | ✅ Implemented |

**Class Diagram**:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                SDS-MOD-009: Cardiac CT Analysis Module                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│   ┌────────────────────────┐   ┌────────────────────────────────────────┐  │
│   │ CardiacPhaseDetector   │   │ CoronaryLineCenterlineExtractor            │  │
│   │                        │   │                                        │  │
│   │ • detectECGGating()    │   │ • computeVesselness(image)             │  │
│   │ • separatePhases()     │   │ • extractCenterline(seed, vesselness)  │  │
│   │ • selectBestPhase()    │   │ • smoothCenterline(path)               │  │
│   │ • getPhaseInfo()       │   │ • measureStenosis(centerline, image)   │  │
│   └───────────┬────────────┘   └──────────────────┬─────────────────────┘  │
│               ↓                                    ↓                        │
│   ┌────────────────────────┐   ┌────────────────────────────────────────┐  │
│   │ CalciumScorer          │   │ CurvedPlanarReformatter                │  │
│   │                        │   │                                        │  │
│   │ • computeAgatston()    │   │ • generateStraightenedCPR()            │  │
│   │ • computeVolumeScore() │   │ • generateCrossSectionalCPR()          │  │
│   │ • classifyRisk()       │   │ • computeStretchedCPR()                │  │
│   │ • assignToArteries()   │   │                                        │  │
│   └────────────────────────┘   └────────────────────────────────────────┘  │
│                                                                               │
│   ┌──────────────────────────────────────────────────────────────────────┐  │
│   │                       CineOrganizer                                    │  │
│   │  • detectCineSeries(dicomFiles) → CineSeriesInfo                      │  │
│   │  • organizePhases(series) → vector<PhaseVolume>                       │  │
│   │  • detectOrientation(series) → "SA"|"2CH"|"3CH"|"4CH"                │  │
│   │  NOTE: Uses TemporalNavigator from SDS-MOD-007 for playback          │  │
│   └──────────────────────────────────────────────────────────────────────┘  │
│                                                                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Key Data Structures**:

```cpp
struct CardiacPhaseInfo {
    int phaseIndex;
    double triggerTime;        // ms from R-wave
    double nominalPercentage;  // % of R-R interval
    std::string phaseLabel;    // "75% diastole", "40% systole"
};

struct CalciumScoreResult {
    double totalAgatston;
    double volumeScore;        // mm³
    double massScore;          // mg
    std::map<std::string, double> perArteryScores;  // "LAD" → score
    std::string riskCategory;  // "None", "Minimal", "Mild", "Moderate", "Severe"
};

struct CenterlinePoint {
    std::array<double, 3> position;
    double radius;             // estimated vessel radius
    std::array<double, 3> tangent;
};

struct CineSeriesInfo {
    int phaseCount;
    int sliceCount;
    double temporalResolution;
    std::string orientation;   // "SA", "2CH", "3CH", "4CH"
    std::vector<double> triggerTimes;
};
```

**Key Design Decisions**:

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Vesselness Filter | Frangi (Hessian-based) | Well-validated for coronary artery detection in literature |
| Centerline Tracking | Minimal path (fast marching) | Robust to noise, produces globally optimal path |
| Phase Separation | Trigger Time grouping | Standard ECG-gated CT acquisition metadata |
| TemporalNavigator Reuse | Composition (not inheritance) | Cardiac CT phases share same navigation pattern as 4D Flow |
| Calcium Threshold | Fixed 130 HU (Agatston standard) | Clinical standard, non-configurable for reproducibility |

---

### SDS-MOD-010: Export Service Module

**Traces to**: SRS-FR-054

**Purpose**: Provide multi-format data export including medical reports, 3D meshes, measurement data, DICOM Structured Reports, CFD interoperability, research data formats, and video generation.

**Components**:

| Component | Description | Traces to | Status |
|-----------|-------------|-----------|--------|
| ReportGenerator | PDF/HTML medical imaging reports with customizable templates | SRS-FR-054.8 | ✅ Implemented |
| DataExporter | NRRD/DICOM volumetric data export with metadata preservation | SRS-FR-054.1 | ✅ Implemented |
| MeasurementSerializer | JSON/CSV measurement serialization with schema validation | SRS-FR-054.3 | ✅ Implemented |
| MeshExporter | STL (binary/ASCII), OBJ (with materials), PLY mesh export | SRS-FR-054.2 | ✅ Implemented |
| DicomSRWriter | DICOM Structured Report generation (SR IOD compliant) | SRS-FR-054.4 | ✅ Implemented |
| EnsightExporter | CFD Ensight Gold format export for external analysis tools | SRS-FR-054.5 | ✅ Implemented |
| MatlabExporter | MATLAB .mat v5 format export for research data | SRS-FR-054.6 | ✅ Implemented |
| VideoExporter | AVI/MP4/MOV video generation from temporal sequences | SRS-FR-054.7 | ✅ Implemented |

**Class Diagram**:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                   SDS-MOD-010: Export Service Module                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│   ┌──────────────────────────┐   ┌──────────────────────────────────────┐  │
│   │ ReportGenerator          │   │ DataExporter                         │  │
│   │                          │   │                                      │  │
│   │ • generatePDF(data)      │   │ • exportNRRD(image, path)            │  │
│   │ • generateHTML(data)     │   │ • exportDICOM(image, metadata, path) │  │
│   │ • loadTemplate(name)     │   │ • exportWithMetadata(image, meta)    │  │
│   │ • embedImages(images)    │   │                                      │  │
│   └──────────────────────────┘   └──────────────────────────────────────┘  │
│                                                                               │
│   ┌──────────────────────────┐   ┌──────────────────────────────────────┐  │
│   │ MeasurementSerializer    │   │ MeshExporter                         │  │
│   │                          │   │                                      │  │
│   │ • toJSON(measurements)   │   │ • exportSTL(mesh, path, binary)      │  │
│   │ • toCSV(measurements)    │   │ • exportOBJ(mesh, materials, path)   │  │
│   │ • fromJSON(json)         │   │ • exportPLY(mesh, path)              │  │
│   │ • validateSchema(json)   │   │ • setCoordinateTransform(matrix)     │  │
│   └──────────────────────────┘   └──────────────────────────────────────┘  │
│                                                                               │
│   ┌──────────────────────────┐   ┌──────────────────────────────────────┐  │
│   │ DicomSRWriter            │   │ EnsightExporter                      │  │
│   │                          │   │                                      │  │
│   │ • createSR(measurements) │   │ • exportCase(velocity, mesh, path)   │  │
│   │ • addCodedTerm(code)     │   │ • writeGeometry(mesh)                │  │
│   │ • addMeasurement(data)   │   │ • writeVariable(field, name)         │  │
│   │ • writeDICOM(path)       │   │ • writeTimesteps(times)              │  │
│   └──────────────────────────┘   └──────────────────────────────────────┘  │
│                                                                               │
│   ┌──────────────────────────┐   ┌──────────────────────────────────────┐  │
│   │ MatlabExporter           │   │ VideoExporter                        │  │
│   │                          │   │                                      │  │
│   │ • exportMat(data, path)  │   │ • setFormat(AVI/MP4/MOV)             │  │
│   │ • addMatrix(name, data)  │   │ • setFPS(fps)                        │  │
│   │ • addStruct(name, fields)│   │ • setCodec(codec)                    │  │
│   │ • addCellArray(name, arr)│   │ • addFrame(image)                    │  │
│   └──────────────────────────┘   │ • addOverlay(overlay)                │  │
│                                   │ • finalize(path)                     │  │
│                                   └──────────────────────────────────────┘  │
│                                                                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Key Data Structures**:

```cpp
struct ExportConfig {
    std::string outputPath;
    std::string format;        // "nrrd", "dicom", "stl", "obj", "ply", "json", "csv"
    bool preserveMetadata;
    std::optional<std::array<std::array<double, 4>, 4>> coordinateTransform;
};

struct ReportConfig {
    std::string templateName;  // "standard", "cardiac", "flow"
    std::string outputFormat;  // "pdf", "html"
    bool embedImages;
    std::vector<std::string> sections;  // sections to include
};

struct VideoConfig {
    std::string format;        // "avi", "mp4", "mov"
    int fps;                   // 1-60
    std::string codec;         // "h264", "mjpeg", "raw"
    int width, height;
    bool includeOverlays;
};
```

**Key Design Decisions**:

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Export Architecture | Strategy pattern per format | Each format has unique requirements; isolates format-specific logic |
| DICOM SR | IOD-compliant generation | Ensures interoperability with clinical systems |
| STL Binary/ASCII | User-selectable mode | Binary for efficiency, ASCII for debugging and compatibility |
| Video Encoding | FFmpeg-based pipeline | Industry standard, wide codec support, cross-platform |
| MATLAB Format | .mat v5 specification | Broad MATLAB/Octave compatibility |

---

## 4. Data Design

### SDS-DATA-001: Image Data Structures

**Traces to**: SRS-DR-001 ~ SRS-DR-005

```cpp
// Core Image Types (include/core/types.hpp)
namespace dicom_viewer {

// ITK Image Types
using PixelType = int16_t;          // CT: signed short (-32768 ~ 32767)
using MaskPixelType = uint8_t;      // Segmentation mask
using FloatPixelType = float;       // Processing

constexpr unsigned int Dimension = 3;

using ImageType = itk::Image<PixelType, Dimension>;
using MaskImageType = itk::Image<MaskPixelType, Dimension>;
using FloatImageType = itk::Image<FloatPixelType, Dimension>;
using RGBImageType = itk::Image<itk::RGBPixel<uint8_t>, Dimension>;

// Smart Pointer Types
using ImagePointer = ImageType::Pointer;
using MaskPointer = MaskImageType::Pointer;
using FloatImagePointer = FloatImageType::Pointer;

// VTK Types
using VTKImagePointer = vtkSmartPointer<vtkImageData>;
using VTKPolyDataPointer = vtkSmartPointer<vtkPolyData>;

} // namespace dicom_viewer
```

---

### SDS-DATA-002: Metadata Structures

**Traces to**: SRS-DR-006 ~ SRS-DR-010

```cpp
// Metadata Structures (include/core/metadata.hpp)
namespace dicom_viewer {

struct PatientInfo {
    std::string patientId;          // (0010,0020)
    std::string patientName;        // (0010,0010)
    std::string birthDate;          // (0010,0030)
    std::string sex;                // (0010,0040)
};

struct StudyInfo {
    std::string studyInstanceUid;   // (0020,000D)
    std::string studyDate;          // (0008,0020)
    std::string studyTime;          // (0008,0030)
    std::string studyDescription;   // (0008,1030)
    std::string accessionNumber;    // (0008,0050)
    std::string referringPhysician; // (0008,0090)
};

struct SeriesInfo {
    std::string seriesInstanceUid;  // (0020,000E)
    std::string modality;           // (0008,0060)
    std::string seriesDescription;  // (0008,103E)
    int seriesNumber;               // (0020,0011)
    int numberOfImages;
    std::array<double, 3> imagePosition;    // (0020,0032)
    std::array<double, 6> imageOrientation; // (0020,0037)
};

struct ImageInfo {
    std::string sopInstanceUid;     // (0008,0018)
    int instanceNumber;             // (0020,0013)
    uint16_t rows;                  // (0028,0010)
    uint16_t columns;               // (0028,0011)
    uint16_t bitsAllocated;         // (0028,0100)
    uint16_t bitsStored;            // (0028,0101)
    uint16_t pixelRepresentation;   // (0028,0103)
    std::array<double, 2> pixelSpacing;  // (0028,0030)
    double sliceThickness;          // (0018,0050)
    double sliceLocation;           // (0020,1041)
    double rescaleSlope;            // (0028,1053)
    double rescaleIntercept;        // (0028,1052)
    double windowCenter;            // (0028,1050)
    double windowWidth;             // (0028,1051)
};

} // namespace dicom_viewer
```

---

### SDS-DATA-003: Segmentation Data Structures

**Traces to**: SRS-DR-011 ~ SRS-DR-015

```cpp
// Segmentation Structures (include/core/segmentation_types.hpp)
namespace dicom_viewer {

// Label Information
struct LabelInfo {
    uint8_t labelValue;
    std::string name;
    QColor color;
    bool visible;
    double opacity;
};

// Segmentation Parameters
struct ThresholdParams {
    double lowerThreshold;
    double upperThreshold;
};

struct RegionGrowParams {
    std::array<int, 3> seedIndex;
    double lowerThreshold;
    double upperThreshold;
    int replaceValue;
};

struct ConfidenceConnectedParams {
    std::array<int, 3> seedIndex;
    double multiplier;          // Standard deviation multiplier
    int numberOfIterations;
    int initialNeighborhoodRadius;
};

struct LevelSetParams {
    double propagationScaling;
    double curvatureScaling;
    double advectionScaling;
    int numberOfIterations;
    double maxRMSError;
};

struct MorphologyParams {
    enum class Operation { Dilate, Erode, Open, Close, FillHoles };
    Operation operation;
    int radius;
};

// Manual Segmentation Tools (SRS-FR-023)
enum class SegmentationTool {
    None, Brush, Eraser, Fill, Freehand, Polygon, SmartScissors
};
enum class BrushShape { Circle, Square };

struct BrushParameters {
    int size = 5;                // 1-50 pixels
    BrushShape shape = BrushShape::Circle;
};

struct FillParameters {
    bool use8Connectivity = false;
    double tolerance = 0.0;
};

struct SmartScissorsParameters {
    double gradientWeight = 0.43;      // Weight for gradient magnitude
    double directionWeight = 0.43;     // Weight for gradient direction
    double laplacianWeight = 0.14;     // Weight for Laplacian zero-crossing
    double gaussianSigma = 1.5;        // Smoothing sigma (1.0-5.0)
    bool enableSmoothing = true;
    double closeThreshold = 10.0;      // Auto-close distance
    bool fillInterior = true;
};

// Segmentation Result
struct SegmentationResult {
    MaskPointer mask;
    std::vector<LabelInfo> labels;
    std::string algorithmUsed;
    std::chrono::milliseconds processingTime;
};

} // namespace dicom_viewer
```

---

### SDS-DATA-004: Measurement Data Structures

**Traces to**: SRS-DR-016 ~ SRS-DR-020

```cpp
// Measurement Structures (include/core/measurement_types.hpp)
namespace dicom_viewer {

// Point Types
using Point2D = std::array<double, 2>;
using Point3D = std::array<double, 3>;

// ROI Types
enum class ROIType { Ellipse, Rectangle, Polygon, Freehand };

struct ROI {
    int id;
    std::string name;
    ROIType type;
    std::vector<Point2D> points;    // Polygon/Freehand
    Point2D center;                 // Ellipse/Rectangle
    Point2D axes;                   // Ellipse (semi-axes) / Rectangle (half-size)
    int sliceIndex;
    QColor color;
    bool visible;
};

// Measurement Types
enum class MeasurementType { Distance, Angle, CobbAngle, Area, Volume };

struct MeasurementResult {
    int id;
    MeasurementType type;
    double value;
    std::string unit;               // mm, mm², cm³, degrees
    std::vector<Point3D> points;    // Points used for measurement
    int sliceIndex;                 // For 2D measurements
    QColor color;
    bool visible;
};

// Statistics
struct Statistics {
    double mean;
    double stdDev;
    double min;
    double max;
    double median;
    int64_t voxelCount;
    double volumeMm3;
    double volumeCm3;
    std::vector<std::pair<double, int64_t>> histogram; // (bin_center, count)
};

// Analysis Report
struct AnalysisReport {
    PatientInfo patient;
    StudyInfo study;
    SeriesInfo series;
    std::vector<SegmentationResult> segmentations;
    std::vector<MeasurementResult> measurements;
    std::vector<Statistics> statistics;
    std::vector<std::string> screenshotPaths;
    std::chrono::system_clock::time_point timestamp;
};

} // namespace dicom_viewer
```

---

### SDS-DATA-005: Transfer Function Data Structures

**Traces to**: SRS-DR-021 ~ SRS-DR-025

{% raw %}
```cpp
// Transfer Function Structures (include/core/transfer_function.hpp)
namespace dicom_viewer {

struct ColorPoint {
    double value;
    double r, g, b;
};

struct OpacityPoint {
    double value;
    double opacity;
};

struct TransferFunctionPreset {
    std::string name;
    std::string category;           // CT, MRI, Custom
    double windowWidth;
    double windowCenter;
    std::vector<ColorPoint> colorPoints;
    std::vector<OpacityPoint> opacityPoints;
    std::vector<OpacityPoint> gradientOpacityPoints;
    bool enableShading;
    double ambient;
    double diffuse;
    double specular;
    double specularPower;
};

// Built-in Presets
const std::vector<TransferFunctionPreset> CT_PRESETS = {
    {"CT Bone", "CT", 2000, 400,
     {{-1000, 0, 0, 0}, {200, 0.8, 0.6, 0.4}, {400, 1, 1, 0.9}, {2000, 1, 1, 1}},
     {{-1000, 0}, {150, 0}, {200, 0.2}, {400, 0.8}, {2000, 1}},
     {}, true, 0.2, 0.8, 0.2, 10},

    {"CT Soft Tissue", "CT", 400, 40,
     {{-160, 0, 0, 0}, {-50, 0.6, 0.3, 0.2}, {40, 0.9, 0.7, 0.5}, {150, 1, 0.9, 0.8}},
     {{-160, 0}, {-50, 0.1}, {40, 0.6}, {150, 0.8}},
     {}, true, 0.3, 0.7, 0.2, 10},

    {"CT Lung", "CT", 1500, -600,
     {{-1000, 0, 0, 0}, {-950, 0.2, 0.2, 0.3}, {-600, 0.5, 0.5, 0.5}, {-400, 0.8, 0.8, 0.8}},
     {{-1000, 0}, {-950, 0.1}, {-600, 0.4}, {-400, 0.6}},
     {}, true, 0.3, 0.6, 0.1, 5},

    {"CT Angio", "CT", 400, 200,
     {{100, 0.8, 0.1, 0.1}, {200, 1, 0.2, 0.2}, {400, 1, 0.5, 0.5}},
     {{100, 0}, {150, 0.3}, {200, 0.7}, {400, 0.9}},
     {}, true, 0.2, 0.8, 0.3, 15}
};

} // namespace dicom_viewer
```
{% endraw %}

---

### SDS-DATA-006: Flow Data Structures

**Traces to**: SRS-FR-043 ~ SRS-FR-048

```cpp
namespace dicom_viewer {

// --- Vendor identification ---

enum class FlowVendorType {
    Unknown,
    Siemens,    // Enhanced MR IOD, (0051,1014)
    Philips,    // Classic MR, (2005,1071) scale slope
    GE          // Classic MR, (0019,10cc)
};

// --- Velocity component classification ---

enum class VelocityComponent {
    Magnitude,  // Magnitude image (no velocity encoding)
    Vx,         // Velocity encoding in X (R/L)
    Vy,         // Velocity encoding in Y (A/P)
    Vz          // Velocity encoding in Z (S/I)
};

// --- Flow DICOM parsing result ---

struct FlowSeriesInfo {
    FlowVendorType vendor;
    int phaseCount;                     // Number of cardiac phases
    float temporalResolution;           // ms between phases
    std::array<float, 3> venc;          // VENC per axis (cm/s)
    bool isSignedPhase;                 // Signed vs unsigned encoding

    // Frame sorting matrix: [phaseIndex][component] → DICOM file path
    std::vector<std::map<VelocityComponent, std::vector<std::string>>> frameMatrix;

    // Metadata
    std::string patientId;
    std::string studyDate;
    std::string seriesDescription;
};

// --- Assembled velocity phase ---

struct VelocityPhase {
    using VectorImageType = itk::VectorImage<float, 3>;     // 3-component (Vx, Vy, Vz)
    using ScalarImageType = itk::Image<float, 3>;

    VectorImageType::Pointer velocityField;    // Corrected velocity (cm/s)
    ScalarImageType::Pointer magnitudeImage;   // Magnitude for masking
    int phaseIndex;                            // Cardiac phase index [0, N-1]
    float triggerTime;                         // ms from R-wave
};

// --- Phase correction configuration ---

struct PhaseCorrectionConfig {
    bool enableAliasingUnwrap = true;
    float unwrapThreshold = 0.8f;       // × VENC

    bool enableEddyCurrentCorrection = true;
    int eddyPolynomialOrder = 2;        // 2nd-order default

    bool enableMaxwellCorrection = false; // Only when gradient info available
};

// --- Flow visualization ---

enum class FlowVisualizationType {
    Streamlines,
    Pathlines,
    VectorGlyphs
};

enum class ColorMappingMode {
    VelocityMagnitude,    // [0, VENC], Rainbow/Jet
    VelocityComponent,    // [-VENC, VENC], Diverging (blue-white-red)
    FlowDirection,        // RGB encoding
    TriggerTime           // [0, RR_interval], Sequential (viridis)
};

struct FlowVisualizationParams {
    FlowVisualizationType type = FlowVisualizationType::Streamlines;
    ColorMappingMode colorMode = ColorMappingMode::VelocityMagnitude;

    // Streamline parameters
    float maxPropagation = 200.0f;      // mm
    float terminalSpeed = 0.1f;         // cm/s
    float tubeRadius = 0.5f;            // mm
    int tubeSides = 8;

    // Glyph parameters
    int skipFactor = 4;                 // Subsample grid for glyphs

    // Seed geometry
    // Defined by: center point + normal (plane), or 3 points, or volume bounds
};

// --- Flow quantification results ---

struct FlowMeasurement {
    float flowRate;                     // mL/s (instantaneous)
    float meanVelocity;                 // cm/s (through-plane mean)
    float maxVelocity;                  // cm/s (through-plane max)
    float area;                         // cm² (vessel cross-section area)
    int phaseIndex;
    float triggerTime;                  // ms
};

struct TimeVelocityCurve {
    std::vector<FlowMeasurement> measurements;  // One per cardiac phase
    float strokeVolume;                 // mL (integral of positive flow)
    float regurgitantVolume;            // mL (integral of negative flow)
    float regurgitantFraction;          // % (regurgitant / stroke × 100)
    float peakVelocity;                 // cm/s
    float meanFlowRate;                 // mL/s
};

// --- Hemodynamic analysis results ---

struct WSSResult {
    using ImageType = itk::Image<float, 3>;
    ImageType::Pointer tawssMap;        // Time-Averaged WSS (Pa)
    ImageType::Pointer osiMap;          // Oscillatory Shear Index [0, 0.5]
    float meanTAWSS;                    // Pa (spatial average)
    float maxTAWSS;                     // Pa
    float meanOSI;
};

struct HemodynamicResults {
    float tke;                          // J/m³ (spatially averaged TKE)
    float pressureGradient;             // mmHg (simplified Bernoulli: 4×V²max)

    using VectorImageType = itk::VectorImage<float, 3>;
    VectorImageType::Pointer vorticityField;     // 1/s
    VectorImageType::Pointer helicityField;      // m/s²
};

// --- Temporal navigation ---

struct PlaybackState {
    int currentPhase = 0;
    int totalPhases = 0;
    bool isPlaying = false;
    float frameRate = 15.0f;            // fps
    float speedMultiplier = 1.0f;       // 0.5x, 1x, 2x, 4x
};

struct CacheStatus {
    std::set<int> cachedPhases;         // Phase indices currently in memory
    size_t memoryUsed = 0;              // bytes
    size_t memoryBudget = 0;            // bytes
    int windowCenter = 0;               // Current center of sliding window
    int windowSize = 5;                 // Total phases in window (±2)
};

} // namespace dicom_viewer
```

---

## 5. Interface Design

### SDS-IF-001: Service Facade Interfaces — Future Design Reference

**Traces to**: SRS-IF-001 ~ SRS-IF-010

> **NOT IMPLEMENTED — DESIGN REFERENCE ONLY**
>
> The interface classes below (`IImageService`, `IRenderService`, `IMeasurementService`,
> `INetworkService`) are **not implemented** in the current codebase. They represent the
> original facade pattern design and are retained as a reference for potential future
> refactoring toward dependency injection.
>
> **Current Architecture**: The codebase uses **direct component access** — UI and service
> code instantiate individual component classes directly without a facade layer.
> See `include/services/` for actual component headers.

**Design Interface → Actual Component Mapping**:

| Design Interface | Actual Components (Direct Access) |
|-----------------|-----------------------------------|
| `IImageService` | `DicomLoader`, `SeriesBuilder`, `GaussianSmoother`, `AnisotropicDiffusionFilter`, `N4BiasCorrector`, `IsotropicResampler`, `ThresholdSegmenter`, `RegionGrowingSegmenter`, `LevelSetSegmenter`, `WatershedSegmenter`, `MorphologyProcessor` |
| `IRenderService` | `VolumeRenderer`, `SurfaceRenderer`, `MprRenderer`, `ObliqueSliceRenderer`, `TransferFunctionManager` |
| `IMeasurementService` | `LinearMeasurementTool`, `AreaMeasurementTool`, `VolumeMeasurementTool`, `RoiStatistics`, `ShapeAnalyzer`, `ReportGenerator` |
| `INetworkService` | `DicomEchoScu`, `DicomFindScu`, `DicomMoveScu`, `DicomStoreScp`, `PacsConfig` |

<details>
<summary><strong>Future Design Reference — Interface Definitions (click to expand)</strong></summary>

```cpp
// Service Interfaces — FUTURE DESIGN REFERENCE (not implemented)
// The current codebase does NOT use these interfaces.
// See the mapping table above for actual component classes.
namespace dicom_viewer {

// Image Service Interface
class IImageService {
public:
    virtual ~IImageService() = default;

    // Loading
    virtual Result<ImageData> loadSeries(const std::filesystem::path& directory) = 0;
    virtual Result<ImageData> loadFile(const std::filesystem::path& file) = 0;

    // Preprocessing
    virtual ImagePointer applyGaussianSmoothing(ImagePointer input, double sigma) = 0;
    virtual ImagePointer applyAnisotropicDiffusion(ImagePointer input,
        int iterations, double conductance) = 0;
    virtual ImagePointer applyN4BiasCorrection(ImagePointer input, MaskPointer mask) = 0;
    virtual ImagePointer resampleIsotropic(ImagePointer input, double spacing) = 0;

    // Segmentation
    virtual MaskPointer segmentThreshold(ImagePointer input,
        const ThresholdParams& params) = 0;
    virtual MaskPointer segmentOtsu(ImagePointer input, int numThresholds) = 0;
    virtual MaskPointer segmentRegionGrow(ImagePointer input,
        const RegionGrowParams& params) = 0;
    virtual MaskPointer segmentConfidenceConnected(ImagePointer input,
        const ConfidenceConnectedParams& params) = 0;
    virtual MaskPointer segmentLevelSet(ImagePointer input,
        MaskPointer initialMask, const LevelSetParams& params) = 0;

    // Morphology
    virtual MaskPointer applyMorphology(MaskPointer input,
        const MorphologyParams& params) = 0;
    virtual MaskPointer fillHoles(MaskPointer input) = 0;
    virtual MaskPointer removeSmallIslands(MaskPointer input, int minSize) = 0;
    virtual MaskPointer keepLargestComponent(MaskPointer input) = 0;
};

// Render Service Interface
class IRenderService {
public:
    virtual ~IRenderService() = default;

    virtual void setImageData(VTKImagePointer data) = 0;
    virtual void setMaskData(VTKImagePointer mask) = 0;
    virtual void setRenderWindow(vtkRenderWindow* window) = 0;

    // Volume Rendering
    virtual void renderVolume() = 0;
    virtual void setVolumePreset(const TransferFunctionPreset& preset) = 0;
    virtual void setClippingBox(const std::array<double, 6>& bounds) = 0;
    virtual void enableMIP(bool enable) = 0;

    // Surface Rendering
    virtual void renderSurface(double threshold, const QColor& color, double opacity) = 0;
    virtual void renderMultiSurface(const std::vector<SurfaceParams>& surfaces) = 0;
    virtual void exportSurface(const std::filesystem::path& path, SurfaceFormat format) = 0;

    // MPR
    virtual void renderMPR(MPROrientation orientation, int slice) = 0;
    virtual void setWindowLevel(double window, double level) = 0;
    virtual void enableCrosshairSync(bool enable) = 0;
    virtual void setThickSlab(SlabMode mode, double thickness) = 0;

    // 2D View
    virtual void render2D(int slice) = 0;
};

// Measurement Service Interface
class IMeasurementService {
public:
    virtual ~IMeasurementService() = default;

    // Linear Measurements
    virtual MeasurementResult measureDistance(const Point3D& p1, const Point3D& p2) = 0;
    virtual MeasurementResult measureAngle(const Point3D& p1, const Point3D& p2,
        const Point3D& p3) = 0;
    virtual MeasurementResult measureCobbAngle(const std::vector<Point3D>& points) = 0;

    // Area Measurements
    virtual ROI createEllipseROI(const Point2D& center, const Point2D& axes, int slice) = 0;
    virtual ROI createPolygonROI(const std::vector<Point2D>& points, int slice) = 0;
    virtual ROI createFreehandROI(const std::vector<Point2D>& points, int slice) = 0;
    virtual double calculateROIArea(const ROI& roi) = 0;
    virtual double calculateROIPerimeter(const ROI& roi) = 0;

    // Volume Measurements
    virtual double calculateMaskVolume(MaskPointer mask, uint8_t label) = 0;
    virtual double calculateMaskSurfaceArea(MaskPointer mask, uint8_t label) = 0;

    // Statistics
    virtual Statistics calculateStatistics(ImagePointer image, MaskPointer mask,
        uint8_t label) = 0;
    virtual Statistics calculateROIStatistics(ImagePointer image, const ROI& roi) = 0;

    // ROI Management
    virtual int addROI(const ROI& roi) = 0;
    virtual void removeROI(int id) = 0;
    virtual void updateROI(int id, const ROI& roi) = 0;
    virtual std::vector<ROI> getAllROIs() = 0;
    virtual void saveROIs(const std::filesystem::path& path) = 0;
    virtual void loadROIs(const std::filesystem::path& path) = 0;

    // Report
    virtual AnalysisReport generateReport() = 0;
    virtual void exportReportPDF(const std::filesystem::path& path) = 0;
    virtual void exportReportCSV(const std::filesystem::path& path) = 0;
};

// Network Service Interface
class INetworkService {
public:
    virtual ~INetworkService() = default;

    virtual bool verifyConnection(const PACSServerConfig& server) = 0;
    virtual std::vector<PatientInfo> queryPatients(const PACSServerConfig& server,
        const QueryCriteria& criteria) = 0;
    virtual std::vector<StudyInfo> queryStudies(const PACSServerConfig& server,
        const std::string& patientId) = 0;
    virtual std::vector<SeriesInfo> querySeries(const PACSServerConfig& server,
        const std::string& studyUid) = 0;
    virtual bool retrieveStudy(const PACSServerConfig& server,
        const std::string& studyUid, const std::filesystem::path& destDir) = 0;
    virtual bool startStorageSCP(uint16_t port,
        const std::filesystem::path& storageDir) = 0;
    virtual void stopStorageSCP() = 0;

    // Configuration
    virtual std::vector<PACSServerConfig> getServerList() = 0;
    virtual void addServer(const PACSServerConfig& config) = 0;
    virtual void removeServer(const std::string& name) = 0;
};

} // namespace dicom_viewer
```

</details>

---

### SDS-IF-002: Signal/Slot Interfaces (Qt) — Design Reference

**Traces to**: SRS-IF-011 ~ SRS-IF-015

> **Design Reference**: The consolidated signal classes below (`ViewportSignals`,
> `PatientBrowserSignals`, `ToolsPanelSignals`) are **not implemented** as separate classes.
> In the current codebase, Qt signals are defined **directly within each panel/widget class**
> (e.g., `SegmentationPanel::toolChanged`, `StatisticsPanel::exportRequested`,
> `PatientBrowser::seriesSelected`). No separate `signals.hpp` file exists.

```cpp
// UI Signal/Slot Interfaces — DESIGN REFERENCE (not implemented as separate classes)
// Actual signals are defined within individual panel/widget classes.
namespace dicom_viewer {

// Viewport Signals
class ViewportSignals : public QObject {
    Q_OBJECT
signals:
    void sliceChanged(int slice, MPROrientation orientation);
    void windowLevelChanged(double window, double level);
    void cursorPositionChanged(const Point3D& position);
    void pixelValueChanged(double value);
    void roiSelected(int roiId);
    void measurementCompleted(const MeasurementResult& result);
    void segmentationCompleted(const SegmentationResult& result);
};

// Patient Browser Signals
class PatientBrowserSignals : public QObject {
    Q_OBJECT
signals:
    void seriesSelected(const std::string& seriesUid);
    void seriesDoubleClicked(const std::string& seriesUid);
    void studySelected(const std::string& studyUid);
};

// Tools Panel Signals
class ToolsPanelSignals : public QObject {
    Q_OBJECT
signals:
    void presetChanged(const std::string& presetName);
    void windowLevelChanged(double window, double level);
    void toolSelected(ToolType tool);
    void segmentationRequested(const SegmentationParams& params);
    void morphologyRequested(const MorphologyParams& params);
    void labelSelected(int labelId);
};

} // namespace dicom_viewer
```

---

## 6. Sequence Diagrams

### SDS-SEQ-001: DICOM Series Loading Sequence

**Traces to**: SRS-FR-001, SRS-FR-002, SRS-FR-003, SRS-FR-004

#### Mermaid Version

```mermaid
sequenceDiagram
    autonumber
    actor User
    participant MW as MainWindow
    participant IS as ImageService
    participant DL as DicomLoader
    participant IB as ImageBridge

    User->>MW: Open Directory
    MW->>IS: loadSeries(path)
    IS->>DL: scanDirectory(path)

    loop for each *.dcm file
        DL->>DL: pacs::dicom_file::open()
        DL->>DL: extractSliceInfo()
    end

    DL-->>IS: SliceInfo[]
    IS->>DL: sortSlices(instanceNumber)
    IS->>DL: buildVolume()
    DL-->>IS: ITK Image<short,3>

    IS->>IB: toVTK(itkImage)
    IB-->>IS: vtkImageData

    IS-->>MW: ImageData
    MW->>MW: updateViews()
    MW-->>User: Display Volume

    Note over DL,IB: HU Conversion: StoredValue × RescaleSlope + RescaleIntercept
```

#### ASCII Version

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    DICOM Series Loading Sequence                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│   User        MainWindow      ImageService      DicomLoader      ImageBridge │
│    │              │                │                │                │       │
│    │  Open Dir    │                │                │                │       │
│    │─────────────>│                │                │                │       │
│    │              │  loadSeries()  │                │                │       │
│    │              │───────────────>│                │                │       │
│    │              │                │  scanDirectory()                │       │
│    │              │                │───────────────>│                │       │
│    │              │                │                │                │       │
│    │              │                │  for each file:│                │       │
│    │              │                │  ┌─────────────┤                │       │
│    │              │                │  │ pacs::dicom_file::open()     │       │
│    │              │                │  │ extractSliceInfo()           │       │
│    │              │                │  └─────────────┤                │       │
│    │              │                │                │                │       │
│    │              │                │  SliceInfo[]   │                │       │
│    │              │                │<───────────────│                │       │
│    │              │                │                │                │       │
│    │              │                │  sortSlices()  │                │       │
│    │              │                │───────────────>│                │       │
│    │              │                │                │                │       │
│    │              │                │  buildVolume() │                │       │
│    │              │                │───────────────>│                │       │
│    │              │                │                │                │       │
│    │              │                │  ITK Image     │                │       │
│    │              │                │<───────────────│                │       │
│    │              │                │                │                │       │
│    │              │                │  toVTK()       │                │       │
│    │              │                │────────────────────────────────>│       │
│    │              │                │                │                │       │
│    │              │                │  VTK ImageData │                │       │
│    │              │                │<────────────────────────────────│       │
│    │              │                │                │                │       │
│    │              │   ImageData    │                │                │       │
│    │              │<───────────────│                │                │       │
│    │              │                │                │                │       │
│    │              │  updateViews() │                │                │       │
│    │              │─────┐          │                │                │       │
│    │              │     │          │                │                │       │
│    │              │<────┘          │                │                │       │
│    │   Display    │                │                │                │       │
│    │<─────────────│                │                │                │       │
│    │              │                │                │                │       │
│                                                                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

### SDS-SEQ-002: Segmentation Workflow Sequence

**Traces to**: SRS-FR-020 ~ SRS-FR-025, SRS-FR-042

#### Mermaid Version

```mermaid
sequenceDiagram
    autonumber
    actor User
    participant SP as SegPanel
    participant TC as ToolController
    participant IS as ImageService
    participant SG as Segmentor
    participant RS as RenderService

    User->>SP: Select Algorithm (Region Growing)
    User->>SP: Set Seed Point
    SP->>TC: onSeedPoint(x, y, z)

    User->>SP: Execute Segmentation
    SP->>TC: segment()
    TC->>IS: regionGrow(seedPoint, params)
    IS->>SG: execute(image, seed, threshold)

    Note over SG: ITK ConnectedThreshold<br/>or NeighborhoodConnected

    SG-->>IS: Mask (itk::Image<uint8_t,3>)
    IS-->>TC: MaskData

    TC->>RS: setMaskData(mask)
    TC->>RS: renderOverlay(mask, color)
    RS-->>SP: Result (overlaid view)

    User->>SP: Apply Morphology (Fill Holes)
    SP->>TC: morphology(FILL_HOLES)
    TC->>IS: fillHoles(mask)
    IS-->>TC: Updated Mask

    TC->>RS: updateOverlay()
    RS-->>User: Display Final Mask
```

#### ASCII Version

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      Segmentation Workflow Sequence                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│   User     SegPanel    ToolCtrl   ImageService   Segmentor    RenderService │
│    │          │           │            │             │              │        │
│    │ Select   │           │            │             │              │        │
│    │ Algorithm│           │            │             │              │        │
│    │─────────>│           │            │             │              │        │
│    │          │           │            │             │              │        │
│    │ Set Seed │           │            │             │              │        │
│    │ Point    │           │            │             │              │        │
│    │─────────>│           │            │             │              │        │
│    │          │  onSeed() │            │             │              │        │
│    │          │──────────>│            │             │              │        │
│    │          │           │            │             │              │        │
│    │ Execute  │           │            │             │              │        │
│    │─────────>│           │            │             │              │        │
│    │          │ segment() │            │             │              │        │
│    │          │──────────>│            │             │              │        │
│    │          │           │ regionGrow()             │              │        │
│    │          │           │───────────>│             │              │        │
│    │          │           │            │  execute()  │              │        │
│    │          │           │            │────────────>│              │        │
│    │          │           │            │             │              │        │
│    │          │           │            │  Mask       │              │        │
│    │          │           │            │<────────────│              │        │
│    │          │           │            │             │              │        │
│    │          │           │ Mask       │             │              │        │
│    │          │           │<───────────│             │              │        │
│    │          │           │            │             │              │        │
│    │          │           │ setMaskData()            │              │        │
│    │          │           │─────────────────────────────────────────>│        │
│    │          │           │            │             │              │        │
│    │          │           │ renderOverlay()          │              │        │
│    │          │           │─────────────────────────────────────────>│        │
│    │          │           │            │             │              │        │
│    │          │ Result    │            │             │              │        │
│    │          │<──────────│            │             │              │        │
│    │          │           │            │             │              │        │
│    │ Apply    │           │            │             │              │        │
│    │ Morphology           │            │             │              │        │
│    │─────────>│           │            │             │              │        │
│    │          │ morph()   │            │             │              │        │
│    │          │──────────>│            │             │              │        │
│    │          │           │ fillHoles()│             │              │        │
│    │          │           │───────────>│             │              │        │
│    │          │           │            │             │              │        │
│    │          │           │ Updated Mask             │              │        │
│    │          │           │<───────────│             │              │        │
│    │          │           │            │             │              │        │
│    │   Display│           │            │             │              │        │
│    │<─────────│           │            │             │              │        │
│    │          │           │            │             │              │        │
│                                                                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

### SDS-SEQ-003: Measurement and Statistics Sequence

**Traces to**: SRS-FR-026 ~ SRS-FR-031

#### Mermaid Version

```mermaid
sequenceDiagram
    autonumber
    actor User
    participant VP as Viewport
    participant TC as ToolController
    participant MS as MeasureService
    participant SC as StatsCalculator
    participant MP as MeasurePanel

    User->>VP: Select ROI Tool (Ellipse)
    VP->>TC: setTool(ROI_ELLIPSE)

    User->>VP: Draw ROI on Image
    VP->>TC: onDraw(points[])

    User->>VP: Complete ROI
    VP->>TC: onComplete()
    TC->>MS: createROI(type, points)
    TC->>MS: addROI(roi)

    MS->>MS: calcArea(roi)
    MS->>SC: calcStats(imageData, roi)

    Note over SC: Statistics Calculation:<br/>Mean, StdDev, Min, Max,<br/>Percentiles

    SC-->>MS: Statistics{mean, std, min, max, ...}
    MS-->>TC: MeasurementResult

    TC->>MP: updatePanel(result)
    MP-->>User: Display Statistics

    VP-->>User: Show ROI Overlay with Values
```

#### ASCII Version

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                   Measurement and Statistics Sequence                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│   User    Viewport    ToolCtrl   MeasureService   StatsCalc    MeasurePanel │
│    │         │           │            │              │              │        │
│    │ Select  │           │            │              │              │        │
│    │ ROI Tool│           │            │              │              │        │
│    │────────>│           │            │              │              │        │
│    │         │ setTool() │            │              │              │        │
│    │         │──────────>│            │              │              │        │
│    │         │           │            │              │              │        │
│    │ Draw ROI│           │            │              │              │        │
│    │────────>│           │            │              │              │        │
│    │         │  onDraw() │            │              │              │        │
│    │         │──────────>│            │              │              │        │
│    │         │           │ createROI()│              │              │        │
│    │         │           │───────────>│              │              │        │
│    │         │           │            │              │              │        │
│    │ Complete│           │            │              │              │        │
│    │ ROI     │           │            │              │              │        │
│    │────────>│           │            │              │              │        │
│    │         │onComplete()            │              │              │        │
│    │         │──────────>│            │              │              │        │
│    │         │           │  addROI()  │              │              │        │
│    │         │           │───────────>│              │              │        │
│    │         │           │            │              │              │        │
│    │         │           │ calcArea() │              │              │        │
│    │         │           │───────────>│              │              │        │
│    │         │           │            │              │              │        │
│    │         │           │ calcStats()│              │              │        │
│    │         │           │───────────>│              │              │        │
│    │         │           │            │  calculate() │              │        │
│    │         │           │            │─────────────>│              │        │
│    │         │           │            │              │              │        │
│    │         │           │            │  Statistics  │              │        │
│    │         │           │            │<─────────────│              │        │
│    │         │           │            │              │              │        │
│    │         │           │ Statistics │              │              │        │
│    │         │           │<───────────│              │              │        │
│    │         │           │            │              │              │        │
│    │         │           │ updatePanel()             │              │        │
│    │         │           │───────────────────────────────────────────>│        │
│    │         │           │            │              │              │        │
│    │ Display │           │            │              │              │        │
│    │<────────│           │            │              │              │        │
│    │         │           │            │              │              │        │
│    │ Show    │           │            │              │              │        │
│    │ Stats   │           │            │              │              │        │
│    │<────────────────────────────────────────────────────────────────│        │
│    │         │           │            │              │              │        │
│                                                                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

### SDS-SEQ-004: Volume Rendering Sequence

**Traces to**: SRS-FR-005, SRS-FR-006

#### Mermaid Version

```mermaid
sequenceDiagram
    autonumber
    actor User
    participant TP as ToolsPanel
    participant RC as RenderController
    participant RS as RenderService
    participant VR as VolumeRenderer
    participant VTK as VTK Pipeline

    User->>TP: Select Preset (e.g., CT_BONE)
    TP->>RC: setPreset(CT_BONE)
    RC->>RS: apply(preset)
    RS->>VR: setPreset(preset)

    VR->>VR: setColorTransferFunction()
    VR->>VTK: AddRGBPoint(hu, r, g, b)

    VR->>VR: setOpacityTransferFunction()
    VR->>VTK: AddPoint(hu, opacity)

    VR->>VR: setShading(ambient, diffuse, specular)

    Note over VR,VTK: GPU Volume Ray Casting:<br/>vtkGPUVolumeRayCastMapper

    VR->>VTK: render()
    VTK->>VTK: Render()

    VTK-->>User: Display Volume

    opt User adjusts W/L
        User->>TP: Adjust Window/Level
        TP->>RC: setWindowLevel(w, l)
        RC->>RS: updateWindowLevel()
        RS->>VR: updateTransferFunctions()
        VR->>VTK: Render()
        VTK-->>User: Updated Display
    end
```

#### ASCII Version

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      Volume Rendering Sequence                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│   User     ToolsPanel   RenderCtrl   RenderService  VolumeRenderer  VTK     │
│    │           │            │             │              │            │      │
│    │ Select    │            │             │              │            │      │
│    │ Preset    │            │             │              │            │      │
│    │──────────>│            │             │              │            │      │
│    │           │ setPreset()│             │              │            │      │
│    │           │───────────>│             │              │            │      │
│    │           │            │  apply()    │              │            │      │
│    │           │            │────────────>│              │            │      │
│    │           │            │             │ setPreset()  │            │      │
│    │           │            │             │─────────────>│            │      │
│    │           │            │             │              │            │      │
│    │           │            │             │  setColorTF()│            │      │
│    │           │            │             │─────────────>│            │      │
│    │           │            │             │              │ AddRGBPoint│      │
│    │           │            │             │              │───────────>│      │
│    │           │            │             │              │            │      │
│    │           │            │             │setOpacityTF()│            │      │
│    │           │            │             │─────────────>│            │      │
│    │           │            │             │              │ AddPoint() │      │
│    │           │            │             │              │───────────>│      │
│    │           │            │             │              │            │      │
│    │           │            │             │ setShading() │            │      │
│    │           │            │             │─────────────>│            │      │
│    │           │            │             │              │            │      │
│    │           │            │             │  render()    │            │      │
│    │           │            │             │─────────────>│            │      │
│    │           │            │             │              │ Render()   │      │
│    │           │            │             │              │───────────>│      │
│    │           │            │             │              │            │      │
│    │   Render  │            │             │              │            │      │
│    │<──────────│            │             │              │            │      │
│    │           │            │             │              │            │      │
│                                                                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

### SDS-SEQ-005: 4D Flow DICOM Load and Assembly Sequence

**Traces to**: SRS-FR-043, SRS-FR-044, SRS-FR-045

#### Mermaid Version

```mermaid
sequenceDiagram
    autonumber
    actor User
    participant UI as MainWindow
    participant FC as FlowController
    participant FDP as FlowDicomParser
    participant VFA as VelocityFieldAssembler
    participant PC as PhaseCorrector
    participant TN as TemporalNavigator

    User->>UI: Open 4D Flow DICOM folder
    UI->>FC: loadFlowSeries(path)
    FC->>FDP: parseFlowSeries(path)

    Note over FDP: Detect vendor from<br/>(0018,0020), (0018,9014)
    FDP->>FDP: detectVendor(dataset)
    FDP->>FDP: selectParser(vendor)

    Note over FDP: Sort frames into<br/>(phase × component) matrix
    FDP->>FDP: classifyComponent(dataset)
    FDP-->>FC: FlowSeriesInfo

    loop For each cardiac phase
        FC->>VFA: assemble(flowInfo, phaseIndex)
        VFA->>VFA: applyVENCScaling(image, venc)
        VFA->>VFA: composeVectorField(vx, vy, vz)
        VFA-->>FC: VelocityPhase (raw)

        FC->>PC: correctAll(velocityPhase)
        PC->>PC: unwrapAliasing(field, venc)
        PC->>PC: correctEddyCurrent(field, magnitude)
        opt Gradient info available
            PC->>PC: correctMaxwellTerms(field, gradientInfo)
        end
        PC-->>FC: VelocityPhase (corrected)
    end

    FC->>TN: initializeCache(phases)
    TN->>TN: loadWindow(currentPhase ± 2)
    FC-->>UI: flowSeriesLoaded(phaseCount, metadata)
    UI-->>User: Display first phase with magnitude overlay
```

#### ASCII Version

```
┌─────────────────────────────────────────────────────────────────────────────┐
│              SDS-SEQ-005: 4D Flow DICOM Load and Assembly                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│ User    UI          FlowCtrl    FlowParser    Assembler    Corrector  TempNav│
│  │       │              │            │             │            │        │   │
│  │ Open  │              │            │             │            │        │   │
│  │ 4D    │              │            │             │            │        │   │
│  │ Flow  │              │            │             │            │        │   │
│  │──────>│ loadFlow     │            │             │            │        │   │
│  │       │─────────────>│ parseFlow  │             │            │        │   │
│  │       │              │───────────>│             │            │        │   │
│  │       │              │            │ detect      │            │        │   │
│  │       │              │            │ vendor      │            │        │   │
│  │       │              │            │ sort frames │            │        │   │
│  │       │              │ FlowInfo   │             │            │        │   │
│  │       │              │<───────────│             │            │        │   │
│  │       │              │                          │            │        │   │
│  │       │              │ ┌─── For each phase ───┐ │            │        │   │
│  │       │              │ │ assemble             │ │            │        │   │
│  │       │              │ │────────────────────> ││ │            │        │   │
│  │       │              │ │ VelocityPhase (raw)  │ │            │        │   │
│  │       │              │ │<──────────────────── ││ │            │        │   │
│  │       │              │ │ correctAll           │ │            │        │   │
│  │       │              │ │──────────────────────│─│───────────>│        │   │
│  │       │              │ │ VelocityPhase (ok)   │ │            │        │   │
│  │       │              │ │<─────────────────────│─│────────────│        │   │
│  │       │              │ └──────────────────────┘ │            │        │   │
│  │       │              │                          │            │        │   │
│  │       │              │ initCache                │            │        │   │
│  │       │              │──────────────────────────│────────────│──────> │   │
│  │       │ loaded       │                          │            │        │   │
│  │       │<─────────────│                          │            │        │   │
│  │ Show  │              │                          │            │        │   │
│  │<──────│              │                          │            │        │   │
│  │       │              │                          │            │        │   │
│                                                                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

### SDS-SEQ-006: Flow Visualization Pipeline Sequence

**Traces to**: SRS-FR-046, SRS-FR-048

#### Mermaid Version

```mermaid
sequenceDiagram
    autonumber
    actor User
    participant UI as MainWindow
    participant FC as FlowController
    participant FV as FlowVisualizer
    participant TN as TemporalNavigator
    participant VP as ViewportWidget

    User->>UI: Select visualization type (Streamlines)
    UI->>FC: setVisualization(Streamlines, colorMode)

    FC->>TN: getCurrentPhase()
    TN-->>FC: VelocityPhase

    FC->>FV: renderStreamlines(field, seedGeometry)

    Note over FV: VTK Pipeline:<br/>PointSource → StreamTracer<br/>→ TubeFilter → Mapper
    FV->>FV: createSeedPoints(geometry)
    FV->>FV: configure StreamTracer (RK45)
    FV->>FV: apply TubeFilter (r=0.5mm)
    FV->>FV: setColorMapping(mode)
    FV-->>FC: vtkActor*

    FC->>VP: addActor(streamlineActor)
    VP->>VP: render()
    VP-->>User: Display streamlines

    Note over User, VP: Phase navigation triggers update
    User->>TN: setPhase(nextIndex)
    TN->>TN: prefetchAdjacentPhases()
    TN-->>FC: phaseChanged(newPhase)
    FC->>FV: renderStreamlines(newField, seeds)
    FV-->>FC: vtkActor* (updated)
    FC->>VP: updateActor(streamlineActor)
    VP-->>User: Display updated streamlines
```

#### ASCII Version

```
┌─────────────────────────────────────────────────────────────────────────────┐
│            SDS-SEQ-006: Flow Visualization Pipeline                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│ User    UI        FlowCtrl    Visualizer    TempNav    Viewport              │
│  │       │            │            │            │          │                  │
│  │Select │            │            │            │          │                  │
│  │Stream │            │            │            │          │                  │
│  │──────>│setVis      │            │            │          │                  │
│  │       │───────────>│getPhase    │            │          │                  │
│  │       │            │────────────│───────────>│          │                  │
│  │       │            │ VelocPhase │            │          │                  │
│  │       │            │<───────────│────────────│          │                  │
│  │       │            │ render     │            │          │                  │
│  │       │            │───────────>│            │          │                  │
│  │       │            │            │ VTK        │          │                  │
│  │       │            │            │ Pipeline   │          │                  │
│  │       │            │ vtkActor   │            │          │                  │
│  │       │            │<───────────│            │          │                  │
│  │       │            │ addActor   │            │          │                  │
│  │       │            │────────────│────────────│─────────>│                  │
│  │       │            │            │            │  render  │                  │
│  │Display│            │            │            │          │                  │
│  │<──────│────────────│────────────│────────────│──────────│                  │
│  │       │            │            │            │          │                  │
│  │ Phase │            │            │            │          │                  │
│  │ Next  │            │            │            │          │                  │
│  │───────│────────────│────────────│───────────>│          │                  │
│  │       │            │phaseChanged│            │          │                  │
│  │       │            │<───────────│────────────│          │                  │
│  │       │            │ re-render  │            │          │                  │
│  │       │            │───────────>│            │          │                  │
│  │       │            │ updated    │            │          │                  │
│  │       │            │<───────────│            │          │                  │
│  │       │            │ update     │            │          │                  │
│  │       │            │────────────│────────────│─────────>│                  │
│  │Updated│            │            │            │          │                  │
│  │<──────│────────────│────────────│────────────│──────────│                  │
│                                                                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

### SDS-SEQ-007: Flow Quantification Sequence

**Traces to**: SRS-FR-047, SRS-FR-048

#### Mermaid Version

```mermaid
sequenceDiagram
    autonumber
    actor User
    participant UI as MainWindow
    participant FC as FlowController
    participant FQ as FlowQuantifier
    participant VA as VesselAnalyzer
    participant TN as TemporalNavigator
    participant SP as StatisticsPanel

    User->>UI: Place measurement plane on vessel
    UI->>FC: measureFlow(planeCenter, planeNormal)

    FC->>TN: getAllCachedPhases()
    TN-->>FC: vector<VelocityPhase>

    loop For each cardiac phase
        FC->>FQ: computeFlowRate(field, plane, contour)
        FQ->>FQ: extractThroughPlaneVelocity(field, plane)
        FQ->>FQ: integrateSurface(velocities, areas)
        FQ-->>FC: FlowMeasurement
    end

    FC->>FQ: computeTimeVelocityCurve(measurements)
    FQ-->>FC: TimeVelocityCurve (SV, RF, peak)

    FC->>SP: displayTVC(timeVelocityCurve)
    SP-->>User: Show time-velocity curve chart

    opt Advanced analysis requested
        User->>UI: Request WSS analysis
        UI->>FC: analyzeVessel(vesselMesh)
        FC->>VA: computeWSS(phases, vesselMesh)
        VA->>VA: sampleNearWallVelocity(field, mesh)
        VA-->>FC: WSSResult (TAWSS, OSI maps)

        FC->>VA: computeTKE(phases)
        VA-->>FC: TKE map

        FC->>SP: displayHemodynamics(wss, tke)
        SP-->>User: Show WSS/OSI/TKE maps and statistics
    end

    opt Export results
        User->>UI: Export to CSV
        FC->>FC: exportFlowResults(tvc, wss, path)
        FC-->>User: CSV file saved
    end
```

#### ASCII Version

```
┌─────────────────────────────────────────────────────────────────────────────┐
│              SDS-SEQ-007: Flow Quantification                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│ User    UI        FlowCtrl    Quantifier   VesselAnlz   TempNav   StatsPane│
│  │       │            │            │            │           │          │     │
│  │Place  │            │            │            │           │          │     │
│  │Plane  │            │            │            │           │          │     │
│  │──────>│measureFlow │            │            │           │          │     │
│  │       │───────────>│getPhases   │            │           │          │     │
│  │       │            │────────────│────────────│──────────>│          │     │
│  │       │            │ phases     │            │           │          │     │
│  │       │            │<───────────│────────────│───────────│          │     │
│  │       │            │            │            │           │          │     │
│  │       │            │ ┌── Per phase ────────┐ │           │          │     │
│  │       │            │ │ flowRate            │ │           │          │     │
│  │       │            │ │────────────────────>││ │           │          │     │
│  │       │            │ │ FlowMeasurement     │ │           │          │     │
│  │       │            │ │<────────────────────││ │           │          │     │
│  │       │            │ └─────────────────────┘ │           │          │     │
│  │       │            │            │            │           │          │     │
│  │       │            │ computeTVC │            │           │          │     │
│  │       │            │───────────>│            │           │          │     │
│  │       │            │ TVC        │            │           │          │     │
│  │       │            │<───────────│            │           │          │     │
│  │       │            │ displayTVC │            │           │          │     │
│  │       │            │────────────│────────────│───────────│─────────>│     │
│  │ TVC   │            │            │            │           │          │     │
│  │<──────│────────────│────────────│────────────│───────────│──────────│     │
│  │       │            │            │            │           │          │     │
│  │ WSS?  │            │            │            │           │          │     │
│  │──────>│analyzeVessl│            │            │           │          │     │
│  │       │───────────>│ WSS        │            │           │          │     │
│  │       │            │────────────│───────────>│           │          │     │
│  │       │            │ WSSResult  │            │           │          │     │
│  │       │            │<───────────│────────────│           │          │     │
│  │       │            │ displayHemo│            │           │          │     │
│  │       │            │────────────│────────────│───────────│─────────>│     │
│  │ Maps  │            │            │            │           │          │     │
│  │<──────│────────────│────────────│────────────│───────────│──────────│     │
│                                                                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 7. Traceability Matrix

### 7.1 PRD → SRS Traceability

| PRD Requirement | SRS Requirement(s) | Priority |
|-----------------|-------------------|----------|
| FR-001 (DICOM Loading) | SRS-FR-001, SRS-FR-002, SRS-FR-003, SRS-FR-004 | P0 |
| FR-002 (Volume Rendering) | SRS-FR-005, SRS-FR-006, SRS-FR-007 | P0 |
| FR-003 (MPR) | SRS-FR-008, SRS-FR-009, SRS-FR-010, SRS-FR-011 | P0 |
| FR-004 (Surface Rendering) | SRS-FR-012, SRS-FR-013, SRS-FR-014, SRS-FR-015 | P0 |
| FR-005 (Preprocessing) | SRS-FR-016~019, SRS-FR-041 | P1 |
| FR-006 (Segmentation) | SRS-FR-020~025, SRS-FR-042 | P1 |
| FR-007 (Measurement) | SRS-FR-026~030 | P1 |
| FR-008 (2D Viewing) | SRS-FR-033 | P2 |
| FR-010 (PACS) | SRS-FR-034~038 | P1 |
| FR-011 (UI) | SRS-FR-039, SRS-FR-040 | P1 |
| FR-012 (ROI Management) | SRS-FR-031 | P1 |
| FR-013 (Analysis Report) | SRS-FR-032 | P2 |
| FR-014 (4D Flow MRI) | SRS-FR-043~048 | P1 |
| FR-015 (Enhanced DICOM) | SRS-FR-049 | P0 |
| FR-016 (Cardiac CT) | SRS-FR-050~052 | P1 |
| FR-017 (Cine MRI) | SRS-FR-053 | P2 |

---

### 7.2 SRS → SDS Traceability

| SRS Requirement | SDS Design Element(s) | Module |
|-----------------|----------------------|--------|
| SRS-FR-001 | SDS-MOD-002 (DicomLoader), SDS-CLS-002 | Image Service |
| SRS-FR-002 | SDS-MOD-002 (DicomLoader), SDS-SEQ-001 | Image Service |
| SRS-FR-003 | SDS-MOD-002 (CodecManager) | Image Service |
| SRS-FR-004 | SDS-MOD-002 (HUConverter) | Image Service |
| SRS-FR-005 | SDS-MOD-003 (VolumeRenderer), SDS-SEQ-004 | Render Service |
| SRS-FR-006 | SDS-MOD-003 (TransferFunctionManager) | Render Service |
| SRS-FR-007 | SDS-MOD-003 (VolumeRenderer) | Render Service |
| SRS-FR-008 | SDS-MOD-003 (MPRRenderer) | Render Service |
| SRS-FR-009 | SDS-MOD-003 (MPRRenderer) | Render Service |
| SRS-FR-010 | SDS-MOD-003 (MPRRenderer) | Render Service |
| SRS-FR-011 | SDS-MOD-003 (MPRRenderer) | Render Service |
| SRS-FR-012 | SDS-MOD-003 (SurfaceRenderer) | Render Service |
| SRS-FR-013 | SDS-MOD-003 (SurfaceRenderer) | Render Service |
| SRS-FR-014 | SDS-MOD-003 (SurfaceRenderer) | Render Service |
| SRS-FR-015 | SDS-MOD-003 (SurfaceRenderer) | Render Service |
| SRS-FR-016~019, SRS-FR-041 | SDS-MOD-002 (Preprocessor) | Image Service |
| SRS-FR-020~025, SRS-FR-042 | SDS-MOD-002 (Segmentor), SDS-SEQ-002 | Image Service |
| SRS-FR-026~030 | SDS-MOD-004, SDS-SEQ-003 | Measurement Service |
| SRS-FR-031 | SDS-MOD-004 (ROIManager) | Measurement Service |
| SRS-FR-032 | SDS-MOD-004 (ReportGenerator) | Measurement Service |
| SRS-FR-033 | SDS-MOD-003 (DRViewer) | Render Service |
| SRS-FR-034~038 | SDS-MOD-005 | Network Service |
| SRS-FR-039, SRS-FR-040 | SDS-MOD-006 | UI Module |
| SRS-FR-043 | SDS-MOD-007 (FlowDicomParser), SDS-SEQ-005 | Flow Analysis |
| SRS-FR-044 | SDS-MOD-007 (VelocityFieldAssembler), SDS-SEQ-005 | Flow Analysis |
| SRS-FR-045 | SDS-MOD-007 (PhaseCorrector), SDS-SEQ-005 | Flow Analysis |
| SRS-FR-046 | SDS-MOD-007 (FlowVisualizer), SDS-SEQ-006 | Flow Analysis |
| SRS-FR-047 | SDS-MOD-007 (FlowQuantifier, VesselAnalyzer), SDS-SEQ-007 | Flow Analysis |
| SRS-FR-048 | SDS-MOD-007 (TemporalNavigator), SDS-DATA-006, SDS-SEQ-006, SDS-SEQ-007 | Flow Analysis |
| SRS-FR-049 | SDS-MOD-008 (EnhancedDicomParser, FrameExtractor, FunctionalGroupParser, DimensionIndexSorter, SeriesClassifier) | Enhanced DICOM |
| SRS-FR-050 | SDS-MOD-009 (CardiacPhaseDetector) | Cardiac CT |
| SRS-FR-051 | SDS-MOD-009 (CoronaryLineCenterlineExtractor, CurvedPlanarReformatter) | Cardiac CT |
| SRS-FR-052 | SDS-MOD-009 (CalciumScorer) | Cardiac CT |
| SRS-FR-053 | SDS-MOD-009 (CineOrganizer) + SDS-MOD-007 (TemporalNavigator) | Cardiac CT / Cine MRI |
| SRS-FR-054 | SDS-MOD-010 (ReportGenerator, DataExporter, MeasurementSerializer, MeshExporter, DicomSRWriter, EnsightExporter, MatlabExporter, VideoExporter) | Export Service |
| SRS-FR-055 | SDS-MOD-002 (CenterlineTracer, LevelTracingTool, HollowTool, MaskSmoother, SliceInterpolator, MaskBooleanOperations, SegmentationCommand, SnapshotCommand) | Image Service |
| SRS-FR-056 | SDS-MOD-006 (ProjectManager) | UI Module |

---

### 7.3 Complete Traceability Matrix

| PRD ID | SRS ID | SDS ID | Module | Implementation Status |
|--------|--------|--------|--------|----------------------|
| FR-001.1 | SRS-FR-001 | SDS-MOD-002, SDS-CLS-002 | Core (DicomLoader) | ✅ Implemented |
| FR-001.2 | SRS-FR-002 | SDS-MOD-002, SDS-SEQ-001 | Core (SeriesBuilder) | ✅ Implemented |
| FR-001.3 | SRS-FR-003 | SDS-MOD-002 | Core (TransferSyntaxDecoder) | ✅ Implemented |
| FR-001.4 | SRS-FR-001 | SDS-MOD-001, SDS-DATA-002 | Core (DicomLoader) | ✅ Implemented |
| FR-001.5 | SRS-FR-004 | SDS-MOD-002 | Core (HounsfieldConverter) | ✅ Implemented |
| FR-002.1 | SRS-FR-005 | SDS-MOD-003, SDS-SEQ-004 | Render (VolumeRenderer) | ✅ Implemented |
| FR-002.2 | SRS-FR-006 | SDS-MOD-003, SDS-DATA-005 | Render (TransferFunctionManager) | ✅ Implemented |
| FR-002.3 | SRS-FR-006 | SDS-MOD-003, SDS-DATA-005 | Render (TransferFunctionManager) | ✅ Implemented |
| FR-002.4 | SRS-FR-006 | SDS-MOD-003, SDS-DATA-005 | Render (TransferFunctionManager) | ✅ Implemented |
| FR-002.5 | SRS-FR-006 | SDS-MOD-003 | Render (TransferFunctionManager) | ✅ Implemented |
| FR-002.6 | SRS-FR-007 | SDS-MOD-003 | Render (DRViewer) | ✅ Implemented |
| FR-003.1 | SRS-FR-008 | SDS-MOD-003 | Render (MPRRenderer) | ✅ Implemented |
| FR-003.2 | SRS-FR-008 | SDS-MOD-003 | Render (MPRRenderer) | ✅ Implemented |
| FR-003.3 | SRS-FR-008 | SDS-MOD-003 | Render (MPRRenderer) | ✅ Implemented |
| FR-003.4 | SRS-FR-009 | SDS-MOD-003 | Render (MPRRenderer) | ✅ Implemented |
| FR-003.5 | SRS-FR-010 | SDS-MOD-003 | Render (MPRRenderer) | ✅ Implemented |
| FR-003.6 | SRS-FR-011 | SDS-MOD-003 | Render (ObliqueResliceRenderer) | ✅ Implemented |
| FR-004.1 | SRS-FR-012 | SDS-MOD-003 | Render (SurfaceRenderer) | ✅ Implemented |
| FR-004.2 | SRS-FR-012 | SDS-MOD-003 | Render (SurfaceRenderer) | ✅ Implemented |
| FR-004.3 | SRS-FR-013 | SDS-MOD-003 | Render (SurfaceRenderer) | ✅ Implemented |
| FR-004.4 | SRS-FR-014 | SDS-MOD-003 | Render (SurfaceRenderer) | ✅ Implemented |
| FR-004.5 | SRS-FR-015 | SDS-MOD-003 | Render (SurfaceRenderer) | ✅ Implemented |
| FR-005.1 | SRS-FR-016 | SDS-MOD-002 | Preprocessing (GaussianSmoother) | ✅ Implemented |
| FR-005.2 | SRS-FR-017 | SDS-MOD-002 | Preprocessing (AnisotropicDiffusionFilter) | ✅ Implemented |
| FR-005.3 | SRS-FR-041 | SDS-MOD-002 | Preprocessing (HistogramEqualizer) | ✅ Implemented |
| FR-005.4 | SRS-FR-018 | SDS-MOD-002 | Preprocessing (N4BiasCorrector) | ✅ Implemented |
| FR-005.5 | SRS-FR-019 | SDS-MOD-002 | Preprocessing (IsotropicResampler) | ✅ Implemented |
| FR-006.1~6 | SRS-FR-020~022, SRS-FR-042 | SDS-MOD-002, SDS-SEQ-002 | Segmentation (Threshold, RegionGrowing, LevelSet, Watershed) | ✅ Implemented |
| FR-006.7~12 | SRS-FR-023 | SDS-MOD-002 | Segmentation (ManualSegmentationController) | ✅ Implemented |
| FR-006.13~18 | SRS-FR-024 | SDS-MOD-002, SDS-DATA-003 | Segmentation (LabelManager) | ✅ Implemented |
| FR-006.19~25 | SRS-FR-025 | SDS-MOD-002 | Segmentation (MorphologicalProcessor) | ✅ Implemented |
| FR-007.1~5 | SRS-FR-026 | SDS-MOD-004, SDS-SEQ-003 | Measurement (LinearMeasurementTool) | ✅ Implemented |
| FR-007.6~10 | SRS-FR-027 | SDS-MOD-004, SDS-DATA-004 | Measurement (AreaMeasurementTool) | ✅ Implemented |
| FR-007.11~14 | SRS-FR-029 | SDS-MOD-004 | Measurement (VolumeCalculator) | ✅ Implemented |
| FR-007.15~20 | SRS-FR-028 | SDS-MOD-004 | Measurement (ROIStatistics) | ✅ Implemented |
| FR-007.21~25 | SRS-FR-030 | SDS-MOD-004 | Measurement (ShapeAnalyzer) | ✅ Implemented |
| FR-010.1~5 | SRS-FR-034~038 | SDS-MOD-005 | PACS (DicomFindSCU, DicomMoveSCU, DicomStoreSCP, DicomEchoSCU, PacsConfigManager) | ✅ Implemented |
| FR-011.1~6 | SRS-FR-039, SRS-FR-040 | SDS-MOD-006 | UI (MainWindow, ViewportWidget, Panels, Dialogs) | ✅ Implemented |
| FR-012.1~8 | SRS-FR-031 | SDS-MOD-004 | Measurement (ROIManager) | ✅ Implemented |
| FR-013.1~6 | SRS-FR-032 | SDS-MOD-004 | Measurement (ReportGenerator) | ✅ Implemented |
| FR-014.1~2 | SRS-FR-043 | SDS-MOD-007, SDS-SEQ-005 | Flow (FlowDicomParser) | ✅ Implemented |
| FR-014.3 | SRS-FR-044 | SDS-MOD-007, SDS-SEQ-005 | Flow (VelocityFieldAssembler) | ✅ Implemented |
| FR-014.4 | SRS-FR-045 | SDS-MOD-007, SDS-SEQ-005 | Flow (PhaseCorrector) | ✅ Implemented |
| FR-014.5~8 | SRS-FR-046 | SDS-MOD-007, SDS-SEQ-006 | Flow (FlowVisualizer) | ✅ Implemented |
| FR-014.9~11 | SRS-FR-048 | SDS-MOD-007, SDS-DATA-006 | Flow (TemporalNavigator) | ✅ Implemented |
| FR-014.12~18 | SRS-FR-047 | SDS-MOD-007, SDS-SEQ-007 | Flow (FlowQuantifier) | ✅ Implemented |
| FR-014.19~21 | SRS-FR-047 | SDS-MOD-007, SDS-SEQ-007 | Flow (VesselAnalyzer, Export) | ✅ Implemented |
| FR-015.1~6 | SRS-FR-049 | SDS-MOD-008 | Enhanced DICOM (EnhancedDicomParser, FrameExtractor, FunctionalGroupParser, DimensionIndexSorter, SeriesClassifier) | ✅ Implemented |
| FR-016.1~4 | SRS-FR-050 | SDS-MOD-009 | Cardiac CT (CardiacPhaseDetector) | ✅ Implemented |
| FR-016.5~8 | SRS-FR-051 | SDS-MOD-009 | Cardiac CT (CoronaryLineCenterlineExtractor, CurvedPlanarReformatter) | ✅ Implemented |
| FR-016.9~12 | SRS-FR-052 | SDS-MOD-009 | Cardiac CT (CalciumScorer) | ✅ Implemented |
| FR-016.13~14 | SRS-FR-050 | SDS-MOD-009 | Cardiac CT (CardiacPhaseDetector - EF) | ✅ Implemented |
| FR-017.1~4 | SRS-FR-053 | SDS-MOD-009, SDS-MOD-007 | Cine MRI (CineOrganizer + TemporalNavigator) | ✅ Implemented |
| FR-018.1~2 | SRS-FR-054 | SDS-MOD-010 | Export (DataExporter, MeshExporter) | ✅ Implemented |
| FR-018.3 | SRS-FR-054 | SDS-MOD-010 | Export (MeasurementSerializer, DicomSRWriter) | ✅ Implemented |
| FR-018.4~5 | SRS-FR-054 | SDS-MOD-010 | Export (EnsightExporter, MatlabExporter) | ✅ Implemented |
| FR-018.6 | SRS-FR-054 | SDS-MOD-010 | Export (VideoExporter) | ✅ Implemented |
| FR-018.7 | SRS-FR-054 | SDS-MOD-010 | Export (ReportGenerator) | ✅ Implemented |

---

## 8. File Structure

### 8.1 Project Directory Layout

```
dicom_viewer/
├── CMakeLists.txt
├── vcpkg.json
├── docs/
│   ├── PRD.md
│   ├── SRS.md
│   ├── SDS.md                          # This document
│   └── reference/
│       ├── README.md
│       ├── 01-itk-overview.md
│       ├── 02-vtk-overview.md
│       ├── 03-itk-vtk-integration.md
│       ├── 04-dicom-pipeline.md
│       └── 05-pacs-integration.md
│
├── include/
│   └── dicom_viewer/
│       ├── core/
│       │   ├── dicom_loader.hpp        # SDS-MOD-001
│       │   ├── series_builder.hpp
│       │   ├── transfer_syntax_decoder.hpp
│       │   ├── image_converter.hpp
│       │   ├── hounsfield_converter.hpp
│       │   ├── logging.hpp
│       │   └── platform/
│       │       └── macos_math_fix.hpp
│       │
│       ├── services/
│       │   ├── preprocessing/          # SDS-MOD-002 (preprocessing)
│       │   │   ├── gaussian_smoother.hpp
│       │   │   ├── anisotropic_diffusion_filter.hpp
│       │   │   ├── n4_bias_corrector.hpp
│       │   │   ├── isotropic_resampler.hpp
│       │   │   └── histogram_equalizer.hpp
│       │   ├── segmentation/           # SDS-MOD-002 (segmentation)
│       │   │   ├── threshold_segmenter.hpp
│       │   │   ├── region_growing_segmenter.hpp
│       │   │   ├── level_set_segmenter.hpp
│       │   │   ├── watershed_segmenter.hpp
│       │   │   ├── manual_segmentation_controller.hpp
│       │   │   ├── morphological_processor.hpp
│       │   │   ├── label_manager.hpp
│       │   │   ├── label_map_overlay.hpp
│       │   │   ├── slice_interpolator.hpp
│       │   │   ├── mpr_segmentation_renderer.hpp
│       │   │   ├── centerline_tracer.hpp
│       │   │   ├── level_tracing_tool.hpp
│       │   │   ├── hollow_tool.hpp
│       │   │   ├── mask_smoother.hpp
│       │   │   ├── mask_boolean_operations.hpp
│       │   │   ├── segmentation_command.hpp
│       │   │   ├── snapshot_command.hpp
│       │   │   ├── phase_tracker.hpp
│       │   │   └── ellipse_roi.hpp
│       │   ├── render/                 # SDS-MOD-003
│       │   │   ├── volume_renderer.hpp
│       │   │   └── surface_renderer.hpp
│       │   ├── measurement/            # SDS-MOD-004
│       │   │   ├── linear_measurement_tool.hpp
│       │   │   ├── area_measurement_tool.hpp
│       │   │   ├── roi_statistics.hpp
│       │   │   ├── volume_calculator.hpp
│       │   │   └── shape_analyzer.hpp
│       │   ├── coordinate/
│       │   │   └── mpr_coordinate_transformer.hpp
│       │   ├── pacs/                   # SDS-MOD-005
│       │   │   ├── dicom_echo_scu.hpp
│       │   │   ├── dicom_find_scu.hpp
│       │   │   ├── dicom_move_scu.hpp
│       │   │   ├── dicom_store_scp.hpp
│       │   │   └── pacs_config_manager.hpp
│       │   ├── export/                 # SDS-MOD-010
│       │   │   ├── report_generator.hpp
│       │   │   ├── data_exporter.hpp
│       │   │   ├── measurement_serializer.hpp
│       │   │   ├── mesh_exporter.hpp
│       │   │   ├── dicom_sr_writer.hpp
│       │   │   ├── ensight_exporter.hpp
│       │   │   ├── matlab_exporter.hpp
│       │   │   └── video_exporter.hpp
│       │   ├── enhanced_dicom/           # SDS-MOD-008
│       │   │   ├── enhanced_dicom_parser.hpp
│       │   │   ├── frame_extractor.hpp
│       │   │   ├── functional_group_parser.hpp
│       │   │   └── dimension_index_sorter.hpp
│       │   ├── cardiac/                 # SDS-MOD-009
│       │   │   ├── cardiac_phase_detector.hpp
│       │   │   ├── coronary_centerline_extractor.hpp
│       │   │   ├── curved_planar_reformatter.hpp
│       │   │   ├── calcium_scorer.hpp
│       │   │   └── cine_organizer.hpp
│       │   ├── flow/                    # SDS-MOD-007
│       │   │   ├── flow_dicom_parser.hpp
│       │   │   ├── vendor_parsers/
│       │   │   │   ├── i_vendor_flow_parser.hpp
│       │   │   │   ├── siemens_flow_parser.hpp
│       │   │   │   ├── philips_flow_parser.hpp
│       │   │   │   └── ge_flow_parser.hpp
│       │   │   ├── velocity_field_assembler.hpp
│       │   │   ├── phase_corrector.hpp
│       │   │   ├── flow_visualizer.hpp
│       │   │   ├── flow_quantifier.hpp
│       │   │   ├── vessel_analyzer.hpp
│       │   │   ├── temporal_navigator.hpp
│       │   │   └── phase_cache.hpp
│       │   ├── mpr_renderer.hpp
│       │   ├── oblique_reslice_renderer.hpp
│       │   └── transfer_function_manager.hpp
│       │
│       └── ui/
│           ├── main_window.hpp         # SDS-MOD-006
│           ├── widgets/
│           │   ├── viewport_widget.hpp
│           │   ├── mpr_widget.hpp
│           │   ├── mpr_view_widget.hpp
│           │   ├── dr_viewer.hpp
│           │   ├── phase_slider_widget.hpp
│           │   ├── sp_mode_toggle.hpp
│           │   ├── flow_graph_widget.hpp
│           │   ├── workflow_tab_bar.hpp
│           │   ├── viewport_layout_manager.hpp
│           │   ├── display_3d_controller.hpp
│           │   ├── drop_handler.hpp
│           │   └── intro_page.hpp
│           ├── panels/
│           │   ├── patient_browser.hpp
│           │   ├── tools_panel.hpp
│           │   ├── statistics_panel.hpp
│           │   ├── segmentation_panel.hpp
│           │   ├── overlay_control_panel.hpp
│           │   ├── flow_tool_panel.hpp
│           │   ├── workflow_panel.hpp
│           │   └── report_panel.hpp
│           └── dialogs/
│               ├── settings_dialog.hpp
│               ├── pacs_config_dialog.hpp
│               ├── quantification_window.hpp
│               ├── mask_wizard.hpp
│               ├── mask_wizard_controller.hpp
│               └── video_export_dialog.hpp
│
├── src/
│   ├── app/
│   │   └── main.cpp
│   │
│   ├── core/
│   │   ├── dicom/
│   │   │   ├── dicom_loader.cpp
│   │   │   ├── series_builder.cpp
│   │   │   └── transfer_syntax_decoder.cpp
│   │   ├── image/
│   │   │   ├── image_converter.cpp
│   │   │   └── hounsfield_converter.cpp
│   │   ├── data/
│   │   │   └── patient_data.cpp
│   │   └── logging/
│   │       └── logging.cpp
│   │
│   ├── services/
│   │   ├── preprocessing/
│   │   │   ├── gaussian_smoother.cpp
│   │   │   ├── anisotropic_diffusion_filter.cpp
│   │   │   ├── n4_bias_corrector.cpp
│   │   │   ├── isotropic_resampler.cpp
│   │   │   └── histogram_equalizer.cpp
│   │   ├── segmentation/
│   │   │   ├── threshold_segmenter.cpp
│   │   │   ├── region_growing_segmenter.cpp
│   │   │   ├── level_set_segmenter.cpp
│   │   │   ├── watershed_segmenter.cpp
│   │   │   ├── manual_segmentation_controller.cpp
│   │   │   ├── morphological_processor.cpp
│   │   │   ├── label_manager.cpp
│   │   │   ├── label_map_overlay.cpp
│   │   │   ├── slice_interpolator.cpp
│   │   │   └── mpr_segmentation_renderer.cpp
│   │   ├── render/
│   │   │   ├── volume_renderer.cpp
│   │   │   ├── surface_renderer.cpp
│   │   │   ├── mpr_renderer.cpp
│   │   │   ├── oblique_reslice_renderer.cpp
│   │   │   └── transfer_function.cpp
│   │   ├── measurement/
│   │   │   ├── linear_measurement_tool.cpp
│   │   │   ├── area_measurement_tool.cpp
│   │   │   ├── roi_statistics.cpp
│   │   │   ├── volume_calculator.cpp
│   │   │   └── shape_analyzer.cpp
│   │   ├── coordinate/
│   │   │   └── mpr_coordinate_transformer.cpp
│   │   ├── pacs/
│   │   │   ├── dicom_echo_scu.cpp
│   │   │   ├── dicom_find_scu.cpp
│   │   │   ├── dicom_move_scu.cpp
│   │   │   ├── dicom_store_scp.cpp
│   │   │   └── pacs_config_manager.cpp
│   │   ├── export/
│   │   │   ├── report_generator.cpp
│   │   │   ├── data_exporter.cpp
│   │   │   ├── measurement_serializer.cpp
│   │   │   ├── mesh_exporter.cpp
│   │   │   ├── dicom_sr_writer.cpp
│   │   │   ├── ensight_exporter.cpp
│   │   │   ├── matlab_exporter.cpp
│   │   │   └── video_exporter.cpp
│   │   └── flow/
│   │       ├── flow_dicom_parser.cpp
│   │       ├── vendor_parsers/
│   │       │   ├── siemens_flow_parser.cpp
│   │       │   ├── philips_flow_parser.cpp
│   │       │   └── ge_flow_parser.cpp
│   │       ├── velocity_field_assembler.cpp
│   │       ├── phase_corrector.cpp
│   │       ├── flow_visualizer.cpp
│   │       ├── flow_quantifier.cpp
│   │       ├── vessel_analyzer.cpp
│   │       ├── temporal_navigator.cpp
│   │       └── phase_cache.cpp
│   │
│   ├── controller/
│   │   ├── viewer_controller.cpp    # stub
│   │   ├── loading_controller.cpp   # stub
│   │   ├── rendering_controller.cpp # stub
│   │   └── tool_controller.cpp      # stub
│   │
│   ├── ui/
│   │   ├── main_window.cpp
│   │   ├── viewport_widget.cpp
│   │   ├── patient_browser.cpp
│   │   ├── tools_panel.cpp
│   │   ├── segmentation_panel.cpp
│   │   └── measurement_panel.cpp
│   │
│   └── main.cpp
│
├── resources/
│   ├── icons/
│   ├── presets/
│   │   ├── ct_presets.json
│   │   └── mri_presets.json
│   └── styles/
│       └── dark_theme.qss
│
└── tests/
    ├── unit/
    │   ├── test_dicom_loader.cpp
    │   ├── test_segmentor.cpp
    │   ├── test_measurement.cpp
    │   ├── test_transfer_function.cpp
    │   ├── test_flow_dicom_parser.cpp
    │   ├── test_velocity_field_assembler.cpp
    │   ├── test_phase_corrector.cpp
    │   └── test_flow_quantifier.cpp
    │
    └── integration/
        ├── test_loading_pipeline.cpp
        ├── test_rendering_pipeline.cpp
        ├── test_pacs_integration.cpp
        └── test_flow_pipeline.cpp
```

---

## 9. Appendix

### A. Technology References

| Technology | Documentation | Usage in Project |
|------------|--------------|------------------|
| ITK 5.x | [ITK Guide](https://itk.org/ITKSoftwareGuide/html/) | Image Processing |
| VTK 9.x | [VTK Docs](https://vtk.org/documentation/) | Visualization |
| Qt6 | [Qt Docs](https://doc.qt.io/qt-6/) | GUI Framework |
| pacs_system | See [REF-005](reference/05-pacs-integration.md) | DICOM Processing |

### B. Design Patterns Used

| Pattern | Usage |
|---------|-------|
| **MVC** | UI-Controller-Service Separation |
| **Factory** | CodecFactory, FilterFactory |
| **Strategy** | Segmentation Algorithms, Vendor Flow Parsers |
| **Observer** | Qt Signals/Slots |
| **Adapter** | ImageBridge (ITK↔VTK) |
| **Facade** | Service Layer APIs |

### C. Coding Standards

- C++20 Standard
- Based on Google C++ Style Guide
- Naming Conventions:
  - Classes: PascalCase
  - Functions/Methods: camelCase
  - Variables: camelCase
  - Constants: UPPER_SNAKE_CASE
  - Member Variables: m_camelCase

---

## Version History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1.0 | 2025-12-31 | Development Team | Initial SDS based on SRS 0.1.0 |
| 0.2.0 | 2025-12-31 | Development Team | Added segmentation and measurement module design |
| 0.3.0 | 2026-02-11 | Development Team | Replaced DCMTK with pacs_system for DICOM network operations; version sync with build system |
| 0.4.0 | 2026-02-11 | Development Team | Fixed SRS-FR traceability references throughout (SRS has 42 requirements, not 60); aligned with SRS v0.4.0 |
| 0.5.0 | 2026-02-11 | Development Team | Added SDS-MOD-007 Flow Analysis Module (7 components), SDS-DATA-006 flow data structures, SDS-SEQ-005~007 flow sequence diagrams; updated ARCH-002/003 and traceability matrices for SRS-FR-043~048 |
| 0.6.0 | 2026-02-12 | Development Team | Added SDS-MOD-008 (Enhanced DICOM Module, 4 components), SDS-MOD-009 (Cardiac CT Analysis Module, 5 components); updated traceability matrices for SRS-FR-049~053 |
| 0.7.0 | 2026-02-20 | Development Team | Updated implementation statuses for MOD-007/008/009 to Implemented; added SDS-MOD-010 Export Service Module (8 components); expanded MOD-002 with advanced segmentation tools, MOD-003 with hemodynamic renderers, MOD-006 with 20 additional UI components; updated traceability matrices |

> **Note**: v0.x.x versions are pre-release. Official release starts from v1.0.0.

---

*This document is subject to change based on detailed design reviews and implementation discoveries.*


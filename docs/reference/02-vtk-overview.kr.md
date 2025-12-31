# VTK (Visualization Toolkit) 개요 및 아키텍처

> **Version**: VTK 9.x
> **Last Updated**: 2025-12-31
> **Reference**: [VTK Official Site](https://vtk.org/), [VTK GitHub](https://github.com/Kitware/VTK)

## 1. VTK 소개

### 1.1 개요

**VTK (Visualization Toolkit)** 는 3D 컴퓨터 그래픽스, 이미지 처리, 과학적 시각화를 위한 오픈소스 소프트웨어 시스템입니다. BSD 3-Clause 라이선스로 배포됩니다.

```
┌─────────────────────────────────────────────────────────────────┐
│                      VTK Visualization Pipeline                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   ┌────────┐    ┌────────┐    ┌────────┐    ┌────────────────┐ │
│   │ Source │ →  │ Filter │ →  │ Mapper │ →  │     Actor      │ │
│   │        │    │        │    │        │    │                │ │
│   │ Reader │    │ Process│    │ Geometry│    │ Props in Scene │ │
│   │ Generate│    │ Data   │    │ to GPU  │    │                │ │
│   └────────┘    └────────┘    └────────┘    └────────────────┘ │
│                                                      │          │
│                                                      ↓          │
│                                              ┌────────────────┐ │
│                                              │   Renderer     │ │
│                                              │                │ │
│                                              │ Camera, Lights │ │
│                                              │ RenderWindow   │ │
│                                              └────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

### 1.2 핵심 특징

| 특징 | 설명 |
|------|------|
| **3D 렌더링** | OpenGL 기반 고품질 렌더링 |
| **볼륨 렌더링** | Ray Casting, GPU 가속 |
| **표면 렌더링** | Marching Cubes, Isosurface |
| **상호작용 위젯** | 3D 조작을 위한 위젯 라이브러리 |
| **다중 플랫폼** | Windows, Linux, macOS, 웹(WebAssembly) |

### 1.3 의료 영상 분야 적용

VTK는 다음과 같은 의료 영상 애플리케이션의 핵심입니다:

- **3D Slicer**: 의료 영상 분석 플랫폼
- **ParaView**: 대규모 데이터 시각화
- **MITK**: Medical Imaging Interaction Toolkit
- **ITK-SNAP**: 영상 분할 도구

---

## 2. 아키텍처

### 2.1 렌더링 파이프라인

VTK의 시각화 파이프라인은 크게 **데이터 파이프라인**과 **렌더링 파이프라인**으로 구성됩니다:

```
┌─────────────────────────────────────────────────────────────────┐
│                    VTK Pipeline Architecture                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────── Data Pipeline ───────────┐   ┌── Render Pipeline │
│  │                                     │   │                   │
│  │  vtkDataSource                      │   │  vtkMapper        │
│  │       │                             │   │       │           │
│  │       ↓                             │   │       ↓           │
│  │  vtkAlgorithm (Filter)              │   │  vtkActor         │
│  │       │                             │   │       │           │
│  │       ↓                             │   │       ↓           │
│  │  vtkDataObject                      │→→→│  vtkRenderer      │
│  │  (vtkImageData, vtkPolyData, etc.)  │   │       │           │
│  │                                     │   │       ↓           │
│  └─────────────────────────────────────┘   │  vtkRenderWindow  │
│                                            │       │           │
│                                            │       ↓           │
│                                            │  vtkInteractor    │
│                                            └───────────────────┘
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 핵심 클래스 계층

```
vtkObjectBase
├── vtkObject
│   ├── vtkDataObject
│   │   ├── vtkDataSet
│   │   │   ├── vtkImageData          // 3D 규칙 격자 (CT/MRI)
│   │   │   ├── vtkRectilinearGrid    // 불규칙 간격 격자
│   │   │   ├── vtkStructuredGrid     // 구조적 격자
│   │   │   ├── vtkUnstructuredGrid   // 비구조적 격자
│   │   │   └── vtkPolyData           // 폴리곤 메시
│   │   └── vtkTable, vtkTree, ...
│   │
│   ├── vtkAlgorithm
│   │   ├── vtkImageAlgorithm         // 이미지 처리
│   │   │   ├── vtkImageReader2       // 이미지 읽기
│   │   │   ├── vtkDICOMImageReader   // DICOM 읽기
│   │   │   └── vtkImageGaussianSmooth
│   │   ├── vtkPolyDataAlgorithm      // 폴리곤 처리
│   │   │   ├── vtkMarchingCubes      // 등표면 추출
│   │   │   └── vtkContourFilter
│   │   └── vtkVolumeMapper           // 볼륨 렌더링
│   │
│   ├── vtkMapper
│   │   ├── vtkPolyDataMapper         // 폴리곤 매핑
│   │   ├── vtkDataSetMapper          // 데이터셋 매핑
│   │   └── vtkVolumeMapper           // 볼륨 매핑
│   │       ├── vtkGPUVolumeRayCastMapper
│   │       └── vtkSmartVolumeMapper
│   │
│   ├── vtkProp
│   │   ├── vtkActor                  // 표면 렌더링
│   │   ├── vtkVolume                 // 볼륨 렌더링
│   │   └── vtkActor2D                // 2D 요소
│   │
│   ├── vtkRenderer                   // 장면 렌더러
│   ├── vtkRenderWindow               // 렌더 윈도우
│   └── vtkRenderWindowInteractor     // 상호작용 처리
│
└── vtkSmartPointer<T>                // 스마트 포인터
```

### 2.3 vtkImageData 구조

의료 영상에서 가장 많이 사용되는 데이터 타입:

```cpp
// vtkImageData: 3D 규칙 격자 데이터
class vtkImageData : public vtkDataSet
{
public:
    // 차원 정보
    int* GetDimensions();              // [width, height, depth]
    void GetSpacing(double spacing[3]); // 복셀 간격 (mm)
    void GetOrigin(double origin[3]);   // 원점 좌표

    // 픽셀 접근
    void* GetScalarPointer(int x, int y, int z);
    double GetScalarComponentAsDouble(int x, int y, int z, int component);

    // 스칼라 정보
    int GetScalarType();               // VTK_SHORT, VTK_FLOAT, etc.
    int GetNumberOfScalarComponents(); // 채널 수
};
```

---

## 3. 볼륨 렌더링 (Volume Rendering)

### 3.1 볼륨 렌더링 파이프라인

```
┌─────────────────────────────────────────────────────────────────┐
│                   Volume Rendering Pipeline                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   vtkImageData                                                  │
│        │                                                        │
│        ↓                                                        │
│   ┌─────────────────────────────────────────┐                   │
│   │         vtkVolumeMapper                 │                   │
│   │                                         │                   │
│   │  ┌─────────────────────────────────┐   │                   │
│   │  │ vtkGPUVolumeRayCastMapper       │   │  ← GPU 가속       │
│   │  │ vtkFixedPointVolumeRayCastMapper│   │  ← CPU            │
│   │  │ vtkSmartVolumeMapper            │   │  ← 자동 선택      │
│   │  └─────────────────────────────────┘   │                   │
│   └─────────────────────────────────────────┘                   │
│        │                                                        │
│        ↓                                                        │
│   ┌─────────────────────────────────────────┐                   │
│   │         vtkVolumeProperty               │                   │
│   │                                         │                   │
│   │  - Color Transfer Function              │                   │
│   │  - Opacity Transfer Function            │                   │
│   │  - Gradient Opacity                     │                   │
│   │  - Shading (Ambient, Diffuse, Specular) │                   │
│   └─────────────────────────────────────────┘                   │
│        │                                                        │
│        ↓                                                        │
│   vtkVolume → vtkRenderer → vtkRenderWindow                     │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 Ray Casting 볼륨 렌더링

```cpp
#include <vtkSmartPointer.h>
#include <vtkDICOMImageReader.h>
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkVolumeProperty.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>
#include <vtkVolume.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>

// DICOM 읽기
auto reader = vtkSmartPointer<vtkDICOMImageReader>::New();
reader->SetDirectoryName(dicomDirectory);
reader->Update();

// GPU Ray Casting Mapper
auto volumeMapper = vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
volumeMapper->SetInputConnection(reader->GetOutputPort());
volumeMapper->SetBlendModeToComposite();

// Color Transfer Function (CT용 예시)
auto colorFunc = vtkSmartPointer<vtkColorTransferFunction>::New();
colorFunc->AddRGBPoint(-1000, 0.0, 0.0, 0.0);  // 공기: 검정
colorFunc->AddRGBPoint(-600, 0.62, 0.36, 0.18); // 지방: 갈색
colorFunc->AddRGBPoint(40, 0.88, 0.60, 0.29);   // 피부: 살색
colorFunc->AddRGBPoint(400, 1.0, 1.0, 0.9);     // 뼈: 흰색

// Opacity Transfer Function
auto opacityFunc = vtkSmartPointer<vtkPiecewiseFunction>::New();
opacityFunc->AddPoint(-1000, 0.0);
opacityFunc->AddPoint(-600, 0.0);
opacityFunc->AddPoint(-400, 0.15);
opacityFunc->AddPoint(400, 0.85);
opacityFunc->AddPoint(2000, 1.0);

// Volume Property
auto volumeProperty = vtkSmartPointer<vtkVolumeProperty>::New();
volumeProperty->SetColor(colorFunc);
volumeProperty->SetScalarOpacity(opacityFunc);
volumeProperty->SetInterpolationTypeToLinear();
volumeProperty->ShadeOn();
volumeProperty->SetAmbient(0.4);
volumeProperty->SetDiffuse(0.6);
volumeProperty->SetSpecular(0.2);

// Volume Actor
auto volume = vtkSmartPointer<vtkVolume>::New();
volume->SetMapper(volumeMapper);
volume->SetProperty(volumeProperty);

// 렌더링
auto renderer = vtkSmartPointer<vtkRenderer>::New();
renderer->AddVolume(volume);
renderer->SetBackground(0.1, 0.1, 0.1);

auto renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
renderWindow->AddRenderer(renderer);
renderWindow->SetSize(800, 600);

auto interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
interactor->SetRenderWindow(renderWindow);
interactor->Start();
```

### 3.3 Transfer Function 프리셋

| 프리셋 | 용도 | HU 범위 |
|--------|------|---------|
| **CT Bone** | 뼈 시각화 | 200 ~ 3000 HU |
| **CT Soft Tissue** | 연조직 | -100 ~ 200 HU |
| **CT Lung** | 폐 | -1000 ~ -400 HU |
| **CT Angio** | 혈관 조영 | 100 ~ 500 HU |
| **MRI Default** | MRI 일반 | 신호 강도 기반 |

---

## 4. 표면 렌더링 (Surface Rendering)

### 4.1 Marching Cubes 알고리즘

등표면(Isosurface) 추출을 위한 가장 널리 사용되는 알고리즘:

```cpp
#include <vtkMarchingCubes.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>

// Marching Cubes로 등표면 추출
auto marchingCubes = vtkSmartPointer<vtkMarchingCubes>::New();
marchingCubes->SetInputConnection(reader->GetOutputPort());
marchingCubes->SetValue(0, 400);  // CT에서 뼈 임계값 (HU)
marchingCubes->ComputeNormalsOn();
marchingCubes->ComputeGradientsOff();

// 스무딩 (선택적)
auto smoother = vtkSmartPointer<vtkSmoothPolyDataFilter>::New();
smoother->SetInputConnection(marchingCubes->GetOutputPort());
smoother->SetNumberOfIterations(50);
smoother->SetRelaxationFactor(0.1);

// 폴리곤 개수 감소 (선택적)
auto decimate = vtkSmartPointer<vtkDecimatePro>::New();
decimate->SetInputConnection(smoother->GetOutputPort());
decimate->SetTargetReduction(0.5);  // 50% 감소
decimate->PreserveTopologyOn();

// Mapper와 Actor
auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
mapper->SetInputConnection(decimate->GetOutputPort());
mapper->ScalarVisibilityOff();

auto actor = vtkSmartPointer<vtkActor>::New();
actor->SetMapper(mapper);
actor->GetProperty()->SetColor(0.9, 0.9, 0.8);  // 뼈 색상
actor->GetProperty()->SetOpacity(1.0);
```

### 4.2 다중 등표면 추출

```cpp
// 여러 장기를 동시에 추출
auto contourFilter = vtkSmartPointer<vtkContourFilter>::New();
contourFilter->SetInputConnection(reader->GetOutputPort());
contourFilter->SetValue(0, -500);  // 폐
contourFilter->SetValue(1, 100);   // 연조직
contourFilter->SetValue(2, 400);   // 뼈
contourFilter->GenerateValues(3, -500, 400);  // 또는 범위 지정
```

---

## 5. 2D 슬라이스 뷰어

### 5.1 MPR (Multi-Planar Reconstruction)

```
┌─────────────────────────────────────────────────────────────────┐
│                      MPR Views                                  │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│    ┌────────────┐   ┌────────────┐   ┌────────────┐            │
│    │   Axial    │   │  Coronal   │   │  Sagittal  │            │
│    │   (XY)     │   │   (XZ)     │   │    (YZ)    │            │
│    │            │   │            │   │            │            │
│    │  ───────   │   │    ─┼─    │   │    │──     │            │
│    │  Top-Down  │   │  Front    │   │   Side     │            │
│    └────────────┘   └────────────┘   └────────────┘            │
│                                                                 │
│    vtkImageReslice를 사용하여 임의 평면에서 슬라이스 추출        │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2 vtkImageViewer2 사용

```cpp
#include <vtkImageViewer2.h>
#include <vtkInteractorStyleImage.h>

// 기본 2D 이미지 뷰어
auto viewer = vtkSmartPointer<vtkImageViewer2>::New();
viewer->SetInputConnection(reader->GetOutputPort());
viewer->SetSliceOrientationToXY();  // Axial
viewer->SetSlice(viewer->GetSliceMax() / 2);  // 중간 슬라이스

// Window/Level 설정
viewer->SetColorWindow(400);  // CT 윈도우 폭
viewer->SetColorLevel(40);    // CT 윈도우 레벨

// 렌더 윈도우 연결
auto renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
viewer->SetRenderWindow(renderWindow);

auto interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
viewer->SetupInteractor(interactor);
viewer->Render();
interactor->Start();
```

### 5.3 vtkImageReslice로 MPR 구현

```cpp
#include <vtkImageReslice.h>
#include <vtkMatrix4x4.h>

// 임의 평면으로 리슬라이스
auto reslice = vtkSmartPointer<vtkImageReslice>::New();
reslice->SetInputConnection(reader->GetOutputPort());
reslice->SetOutputDimensionality(2);
reslice->SetInterpolationModeToLinear();

// Axial (기본)
auto axialMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
axialMatrix->DeepCopy(new double[16]{
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, slicePosition,
    0, 0, 0, 1
});
reslice->SetResliceAxes(axialMatrix);

// Coronal
auto coronalMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
coronalMatrix->DeepCopy(new double[16]{
    1, 0, 0, 0,
    0, 0, 1, 0,
    0, -1, 0, slicePosition,
    0, 0, 0, 1
});

// Sagittal
auto sagittalMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
sagittalMatrix->DeepCopy(new double[16]{
    0, 0, 1, 0,
    1, 0, 0, 0,
    0, -1, 0, slicePosition,
    0, 0, 0, 1
});
```

---

## 6. 상호작용 위젯

### 6.1 주요 위젯 클래스

| 위젯 | 용도 |
|------|------|
| `vtkSliderWidget` | 슬라이더 (슬라이스 선택) |
| `vtkPlaneWidget` | 평면 조작 (MPR) |
| `vtkBoxWidget2` | 볼륨 클리핑 |
| `vtkLineWidget2` | 거리 측정 |
| `vtkAngleWidget` | 각도 측정 |
| `vtkImagePlaneWidget` | 이미지 평면 |
| `vtkContourWidget` | 컨투어 그리기 |
| `vtkSeedWidget` | 시드 포인트 |

### 6.2 위젯 사용 예시

```cpp
#include <vtkPlaneWidget.h>
#include <vtkImagePlaneWidget.h>

// 이미지 평면 위젯 (MPR 상호작용)
auto planeWidget = vtkSmartPointer<vtkImagePlaneWidget>::New();
planeWidget->SetInteractor(interactor);
planeWidget->SetInputConnection(reader->GetOutputPort());
planeWidget->SetPlaneOrientationToXAxes();
planeWidget->SetSliceIndex(100);
planeWidget->DisplayTextOn();
planeWidget->On();

// 슬라이더 위젯
auto sliderRep = vtkSmartPointer<vtkSliderRepresentation2D>::New();
sliderRep->SetMinimumValue(0);
sliderRep->SetMaximumValue(reader->GetOutput()->GetDimensions()[2] - 1);
sliderRep->SetValue(50);
sliderRep->SetTitleText("Slice");
sliderRep->GetPoint1Coordinate()->SetCoordinateSystemToNormalizedDisplay();
sliderRep->GetPoint1Coordinate()->SetValue(0.1, 0.1);
sliderRep->GetPoint2Coordinate()->SetCoordinateSystemToNormalizedDisplay();
sliderRep->GetPoint2Coordinate()->SetValue(0.9, 0.1);

auto sliderWidget = vtkSmartPointer<vtkSliderWidget>::New();
sliderWidget->SetInteractor(interactor);
sliderWidget->SetRepresentation(sliderRep);
sliderWidget->EnabledOn();
```

---

## 7. DICOM 처리

### 7.1 vtkDICOMImageReader

VTK의 기본 DICOM 리더 (제한적):

```cpp
auto reader = vtkSmartPointer<vtkDICOMImageReader>::New();
reader->SetDirectoryName(dicomDir);
reader->Update();

// 메타데이터 접근
std::string patientName = reader->GetPatientName();
float* spacing = reader->GetPixelSpacing();
float* origin = reader->GetImagePositionPatient();
int* dimensions = reader->GetOutput()->GetDimensions();
```

### 7.2 vtk-dicom 라이브러리 (권장)

더 완전한 DICOM 지원을 위해 [vtk-dicom](https://github.com/dgobbi/vtk-dicom) 사용 권장:

```cpp
#include <vtkDICOMReader.h>
#include <vtkDICOMSorter.h>
#include <vtkDICOMMetaData.h>

// DICOM 정렬
auto sorter = vtkSmartPointer<vtkDICOMSorter>::New();
sorter->SetInputFileNames(fileNames);
sorter->Update();

// 정렬된 시리즈로 읽기
auto reader = vtkSmartPointer<vtkDICOMReader>::New();
reader->SetFileNames(sorter->GetOutputFileNames());
reader->Update();

// 메타데이터
vtkDICOMMetaData* meta = reader->GetMetaData();
std::string patientID = meta->Get(DC::PatientID).AsString();
std::string studyDate = meta->Get(DC::StudyDate).AsString();
```

---

## 8. Qt 통합

### 8.1 QVTKOpenGLNativeWidget

```cpp
#include <QVTKOpenGLNativeWidget.h>
#include <vtkGenericOpenGLRenderWindow.h>

class DicomViewer : public QMainWindow
{
    Q_OBJECT
public:
    DicomViewer(QWidget* parent = nullptr)
    {
        // Qt 위젯 설정
        m_vtkWidget = new QVTKOpenGLNativeWidget(this);
        setCentralWidget(m_vtkWidget);

        // VTK 렌더 윈도우
        auto renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
        m_vtkWidget->setRenderWindow(renderWindow);

        // 렌더러 추가
        m_renderer = vtkSmartPointer<vtkRenderer>::New();
        renderWindow->AddRenderer(m_renderer);

        // 인터랙터 설정
        auto interactor = m_vtkWidget->interactor();
        auto style = vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
        interactor->SetInteractorStyle(style);
    }

private:
    QVTKOpenGLNativeWidget* m_vtkWidget;
    vtkSmartPointer<vtkRenderer> m_renderer;
};
```

### 8.2 CMake 설정

```cmake
find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets)
find_package(VTK REQUIRED COMPONENTS
    CommonCore
    CommonDataModel
    IOImage
    IOXML
    InteractionStyle
    RenderingCore
    RenderingOpenGL2
    RenderingVolumeOpenGL2
    GUISupportQt
)

target_link_libraries(dicom_viewer PRIVATE
    Qt6::Widgets
    Qt6::OpenGLWidgets
    ${VTK_LIBRARIES}
)

vtk_module_autoinit(
    TARGETS dicom_viewer
    MODULES ${VTK_LIBRARIES}
)
```

---

## 9. 성능 최적화

### 9.1 GPU 가속

| 기능 | 클래스 | 비고 |
|------|--------|------|
| **볼륨 렌더링** | `vtkGPUVolumeRayCastMapper` | CUDA/OpenGL |
| **스마트 매퍼** | `vtkSmartVolumeMapper` | 자동 GPU/CPU 선택 |
| **OpenGL 폴리곤** | `vtkOpenGLPolyDataMapper` | 기본 GPU 가속 |

### 9.2 대용량 데이터 처리

```cpp
// 스트리밍 처리
auto reader = vtkSmartPointer<vtkDICOMImageReader>::New();
reader->FileLowerLeftOn();
reader->SetDataExtent(0, 511, 0, 511, 0, 99);  // 부분 로딩
reader->Update();

// LOD (Level of Detail)
auto lodMapper = vtkSmartPointer<vtkLODProp3D>::New();
lodMapper->AddLOD(highResMapper, 0.0);
lodMapper->AddLOD(lowResMapper, 0.5);
```

---

## 10. 빌드 설정

### 10.1 CMake 설정

```cmake
cmake_minimum_required(VERSION 3.16)
project(DicomViewer)

find_package(VTK REQUIRED COMPONENTS
    # Core
    CommonCore
    CommonDataModel
    CommonExecutionModel
    CommonMath
    CommonTransforms

    # IO
    IOCore
    IOImage
    IOXML

    # Filtering
    FiltersCore
    FiltersGeneral
    FiltersGeometry
    FiltersModeling
    FiltersSources

    # Imaging
    ImagingCore
    ImagingGeneral
    ImagingMath

    # Rendering
    RenderingCore
    RenderingOpenGL2
    RenderingVolume
    RenderingVolumeOpenGL2

    # Interaction
    InteractionStyle
    InteractionWidgets

    # (선택) Qt 통합
    GUISupportQt
)

add_executable(dicom_viewer main.cpp)
target_link_libraries(dicom_viewer PRIVATE ${VTK_LIBRARIES})
vtk_module_autoinit(TARGETS dicom_viewer MODULES ${VTK_LIBRARIES})
```

---

## 11. 참고 자료

### 공식 문서
- [VTK Documentation](https://vtk.org/documentation/)
- [VTK Examples](https://examples.vtk.org/)
- [VTK Doxygen API](https://vtk.org/doc/nightly/html/)

### 의료 영상 특화
- [Kitware VTK for Medical Imaging Training](https://www.kitware.eu/vtk-for-medical-imaging-training/)
- [3D Slicer Developer Guide](https://slicer.readthedocs.io/)

### 관련 라이브러리
- [vtk-dicom](https://github.com/dgobbi/vtk-dicom) - 향상된 DICOM 지원
- [GDCM](https://gdcm.sourceforge.net/) - Grassroots DICOM

---

*이전 문서: [01-itk-overview.md](01-itk-overview.md) - ITK 개요*
*다음 문서: [03-itk-vtk-integration.md](03-itk-vtk-integration.md) - ITK-VTK 통합 가이드*

# 의료 영상용 C++ GUI 프레임워크 비교

> **Last Updated**: 2025-12-31
> **Context**: DICOM Viewer GUI 프레임워크 선정
> **Current Choice**: Qt6 (권장)

## 1. 개요

### 1.1 GUI 프레임워크 현황

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    C++ GUI Framework Landscape                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Traditional (Retained Mode)          Immediate Mode                   │
│   ─────────────────────────           ──────────────                    │
│   ┌─────────┐  ┌─────────┐           ┌─────────┐  ┌─────────┐          │
│   │   Qt    │  │wxWidgets│           │Dear ImGui│  │ Nuklear │          │
│   │  (LGPL) │  │ (LGPL)  │           │  (MIT)  │  │ (Public)│          │
│   └─────────┘  └─────────┘           └─────────┘  └─────────┘          │
│   ┌─────────┐  ┌─────────┐           ┌─────────┐                       │
│   │   GTK   │  │  FLTK   │           │  Slint  │                       │
│   │ (LGPL)  │  │ (LGPL)  │           │(GPL/Com)│                       │
│   └─────────┘  └─────────┘           └─────────┘                       │
│                                                                         │
│   VTK 통합:  Qt ★★★★★  |  ImGui ★★★★☆  |  Others ★★☆☆☆              │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 1.2 Retained Mode vs Immediate Mode

| 구분 | Retained Mode | Immediate Mode |
|------|---------------|----------------|
| **개념** | 위젯이 상태를 유지 | 매 프레임 UI 재생성 |
| **예시** | Qt, wxWidgets, GTK | Dear ImGui, Nuklear |
| **메모리** | 높음 (위젯 객체) | 낮음 (상태 없음) |
| **복잡도** | 복잡한 애플리케이션 | 단순/도구 UI |
| **성능** | 이벤트 기반 | 프레임 기반 (60+ FPS) |
| **학습 곡선** | 높음 | 낮음 |

---

## 2. 프레임워크별 상세 비교

### 2.1 Qt6

```
┌─────────────────────────────────────────────────────────────────┐
│                            Qt6                                   │
├─────────────────────────────────────────────────────────────────┤
│  Type: Retained Mode GUI (Full Framework)                        │
│  License: LGPL v3 / Commercial                                   │
│  Website: qt.io                                                  │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  장점:                              단점:                        │
│  ───────                           ───────                       │
│  ✓ VTK 공식 지원                    ✗ 큰 런타임 (~50MB+)         │
│  ✓ 포괄적인 위젯 세트               ✗ 복잡한 빌드 시스템          │
│  ✓ 우수한 문서화                    ✗ 상용 라이선스 비용          │
│  ✓ 크로스 플랫폼 일관성             ✗ 높은 학습 곡선              │
│  ✓ 강력한 IDE 지원 (Qt Creator)    ✗ MOC 전처리                  │
│  ✓ 성숙한 생태계 (20년+)           ✗ 무거운 의존성               │
│  ✓ 의료 영상 레퍼런스               │                            │
│    (3D Slicer, MITK, ParaView)     │                            │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**VTK 통합:**
```cpp
// Qt6 + VTK: 공식 통합
#include <QVTKOpenGLNativeWidget.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderer.h>

class DicomViewerWidget : public QVTKOpenGLNativeWidget {
    Q_OBJECT
public:
    DicomViewerWidget(QWidget* parent = nullptr)
        : QVTKOpenGLNativeWidget(parent)
    {
        auto renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
        setRenderWindow(renderWindow);

        m_renderer = vtkSmartPointer<vtkRenderer>::New();
        renderWindow->AddRenderer(m_renderer);
    }

    void SetVolumeData(vtkImageData* imageData) {
        // 볼륨 렌더링 설정
        m_volumeMapper->SetInputData(imageData);
        m_renderer->AddVolume(m_volume);
        renderWindow()->Render();
    }

private:
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkGPUVolumeRayCastMapper> m_volumeMapper;
    vtkSmartPointer<vtkVolume> m_volume;
};
```

**의료 영상용 주요 기능:**
- `QVTKOpenGLNativeWidget`: 네이티브 VTK 위젯 통합
- `QVTKOpenGLStereoWidget`: 스테레오 3D 지원
- Signal/Slot 이벤트 처리
- QML 선언적 UI

---

### 2.2 Dear ImGui

```
┌─────────────────────────────────────────────────────────────────┐
│                         Dear ImGui                               │
├─────────────────────────────────────────────────────────────────┤
│  Type: Immediate Mode GUI                                        │
│  License: MIT (상용 사용에 매우 자유로움)                         │
│  Repository: github.com/ocornut/imgui                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  장점:                              단점:                        │
│  ───────                           ───────                       │
│  ✓ 매우 가벼움 (~15K LOC)          ✗ 네이티브 룩앤필 없음        │
│  ✓ 헤더 온리 통합                  ✗ 복잡한 레이아웃 어려움       │
│  ✓ VTK 통합 용이                   ✗ 제한적인 텍스트 편집         │
│  ✓ 실시간 파라미터 튜닝에 최적     ✗ 접근성(A11y) 미지원         │
│  ✓ 빠른 프로토타이핑              ✗ 수동 i18n 구현 필요          │
│  ✓ GPU 렌더링 기반                ✗ 파일 다이얼로그 미내장        │
│  ✓ MIT 라이선스 (무료 상용)       ✗ 복잡한 앱 UI에 부적합         │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**VTK 통합:**
```cpp
// Dear ImGui + VTK 통합
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>

class ImGuiVTKViewer {
public:
    void Initialize(GLFWwindow* window) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 410");

        // VTK 설정
        m_renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
        m_renderWindow->SetWindowId(window);  // OpenGL 컨텍스트 공유
    }

    void Render() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ImGui UI
        RenderControlPanel();

        // VTK 렌더링 (동일 OpenGL 컨텍스트)
        m_renderWindow->Render();

        // ImGui 렌더링 (오버레이)
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

private:
    void RenderControlPanel() {
        ImGui::Begin("Volume Controls");

        if (ImGui::SliderFloat("Window Width", &m_windowWidth, 1, 4000)) {
            UpdateWindowLevel();
        }
        if (ImGui::SliderFloat("Window Level", &m_windowLevel, -1000, 3000)) {
            UpdateWindowLevel();
        }

        ImGui::Separator();
        ImGui::Text("프리셋:");
        if (ImGui::Button("CT Bone")) { SetPreset(Preset::CTBone); }
        ImGui::SameLine();
        if (ImGui::Button("CT Soft Tissue")) { SetPreset(Preset::CTSoftTissue); }

        ImGui::Separator();
        if (ImGui::CollapsingHeader("Transfer Function")) {
            // Transfer function 편집기
            ImGui::PlotLines("Opacity", m_opacityValues, 256);
        }

        ImGui::End();
    }

    vtkSmartPointer<vtkRenderWindow> m_renderWindow;
    float m_windowWidth = 400.0f;
    float m_windowLevel = 40.0f;
    float m_opacityValues[256];
};
```

**최적 활용 사례:**
- 연구용 프로토타입
- 파라미터 튜닝 인터페이스
- 디버그/개발용 오버레이
- 실시간 시각화 도구

---

### 2.3 wxWidgets

```
┌─────────────────────────────────────────────────────────────────┐
│                         wxWidgets                                │
├─────────────────────────────────────────────────────────────────┤
│  Type: Retained Mode GUI (Native Wrapper)                        │
│  License: wxWindows License (LGPL 호환, 상용 친화적)             │
│  Repository: github.com/wxWidgets/wxWidgets                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  장점:                              단점:                        │
│  ───────                           ───────                       │
│  ✓ 진정한 네이티브 룩앤필          ✗ VTK 통합이 복잡             │
│  ✓ 상용 친화적 라이선스            ✗ Qt보다 문서화 부족          │
│  ✓ 성숙한 라이브러리 (30년+)       ✗ 작은 커뮤니티               │
│  ✓ 크로스 플랫폼                   ✗ 현대적 C++ 지원 제한        │
│  ✓ 가벼운 런타임                   ✗ OpenGL 위젯 직접 구현       │
│  ✓ 상용 라이선스 비용 없음         ✗ 의료 영상 레퍼런스 부족     │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**VTK 통합:**
```cpp
// wxWidgets + VTK 통합
#include <wx/wx.h>
#include <wx/glcanvas.h>
#include "wxVTKRenderWindowInteractor.h"
#include <vtkRenderer.h>

class VTKFrame : public wxFrame {
public:
    VTKFrame() : wxFrame(nullptr, wxID_ANY, "DICOM Viewer",
                         wxDefaultPosition, wxSize(1200, 800))
    {
        // VTK 위젯 생성
        m_vtkWidget = new wxVTKRenderWindowInteractor(this, wxID_ANY);

        // 렌더러 설정
        m_renderer = vtkSmartPointer<vtkRenderer>::New();
        m_vtkWidget->GetRenderWindow()->AddRenderer(m_renderer);

        // 레이아웃
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(m_vtkWidget, 1, wxEXPAND);
        SetSizer(sizer);

        // 메뉴 생성
        CreateMenuBar();
    }

private:
    void CreateMenuBar() {
        wxMenuBar* menuBar = new wxMenuBar();

        wxMenu* fileMenu = new wxMenu();
        fileMenu->Append(wxID_OPEN, "&Open DICOM...\tCtrl+O");
        fileMenu->AppendSeparator();
        fileMenu->Append(wxID_EXIT, "E&xit\tAlt+F4");

        menuBar->Append(fileMenu, "&File");
        SetMenuBar(menuBar);

        Bind(wxEVT_MENU, &VTKFrame::OnOpen, this, wxID_OPEN);
    }

    void OnOpen(wxCommandEvent& event) {
        wxDirDialog dialog(this, "DICOM 디렉토리 선택");
        if (dialog.ShowModal() == wxID_OK) {
            LoadDICOMSeries(dialog.GetPath().ToStdString());
        }
    }

    wxVTKRenderWindowInteractor* m_vtkWidget;
    vtkSmartPointer<vtkRenderer> m_renderer;
};
```

---

### 2.4 GTK (gtkmm for C++)

```
┌─────────────────────────────────────────────────────────────────┐
│                      GTK4 / gtkmm                                │
├─────────────────────────────────────────────────────────────────┤
│  Type: Retained Mode GUI                                         │
│  License: LGPL 2.1+                                              │
│  Website: gtk.org                                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  장점:                              단점:                        │
│  ───────                           ───────                       │
│  ✓ Linux/GNOME 네이티브            ✗ Windows/Mac 비네이티브     │
│  ✓ 현대적 UI 디자인 (GTK4)         ✗ VTK 통합 사례 없음          │
│  ✓ 활발한 개발                     ✗ 크로스 플랫폼 일관성 부족   │
│  ✓ CSS 스타일링 지원               ✗ 의료 영상 사례 부족         │
│  ✓ 좋은 접근성 지원                ✗ C++ 바인딩 복잡             │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**VTK 통합 (커스텀):**
```cpp
// GTK4 + VTK는 커스텀 GLArea 통합 필요
#include <gtkmm.h>
#include <vtkRenderWindow.h>

class VTKGtkWidget : public Gtk::GLArea {
public:
    VTKGtkWidget() {
        set_required_version(4, 1);
        signal_realize().connect(sigc::mem_fun(*this, &VTKGtkWidget::OnRealize));
        signal_render().connect(sigc::mem_fun(*this, &VTKGtkWidget::OnRender), false);
    }

protected:
    void OnRealize() {
        make_current();
        m_renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
        // 커스텀 OpenGL 컨텍스트 공유 필요
    }

    bool OnRender(const Glib::RefPtr<Gdk::GLContext>& context) {
        m_renderWindow->Render();
        return true;
    }

private:
    vtkSmartPointer<vtkRenderWindow> m_renderWindow;
};
```

---

### 2.5 FLTK (Fast Light Toolkit)

```
┌─────────────────────────────────────────────────────────────────┐
│                           FLTK                                   │
├─────────────────────────────────────────────────────────────────┤
│  Type: Retained Mode GUI (Lightweight)                           │
│  License: LGPL with static linking exception                     │
│  Repository: github.com/fltk/fltk                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  장점:                              단점:                        │
│  ───────                           ───────                       │
│  ✓ 매우 가벼움 (~500KB)            ✗ 구식 UI 디자인              │
│  ✓ 빠른 컴파일                     ✗ 현대적 위젯 부족            │
│  ✓ OpenGL 통합 용이                ✗ 문서화 부족                 │
│  ✓ 정적 링킹 가능                  ✗ 복잡한 레이아웃 어려움      │
│  ✓ 최소 의존성                     ✗ 활발하지 않은 개발          │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

### 2.6 Slint

```
┌─────────────────────────────────────────────────────────────────┐
│                          Slint                                   │
├─────────────────────────────────────────────────────────────────┤
│  Type: Declarative UI (QML-like)                                 │
│  License: GPL v3 / Commercial                                    │
│  Repository: github.com/slint-ui/slint                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  장점:                              단점:                        │
│  ───────                           ───────                       │
│  ✓ 현대적 선언적 UI                ✗ 상용 라이선스 비용          │
│  ✓ Rust/C++/JS 바인딩              ✗ 초기 단계 (2021~)          │
│  ✓ 실시간 미리보기                 ✗ VTK 통합 사례 없음          │
│  ✓ 임베디드 지원                   ✗ 작은 커뮤니티/생태계        │
│  ✓ 작은 런타임                     ✗ 제한적인 위젯 세트          │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. 종합 비교표

### 3.1 기능 비교

| 기준 | Qt6 | Dear ImGui | wxWidgets | GTK4 | FLTK | Slint |
|------|:---:|:---:|:---:|:---:|:---:|:---:|
| **VTK 공식 지원** | ✅ | ⚠️ | ⚠️ | ❌ | ⚠️ | ❌ |
| **크로스 플랫폼** | ✅ | ✅ | ✅ | ⚠️ | ✅ | ✅ |
| **네이티브 룩** | ⚠️ | ❌ | ✅ | ⚠️ | ❌ | ❌ |
| **라이선스** | LGPL/상용 | MIT | LGPL+ | LGPL | LGPL+ | GPL/상용 |
| **학습 곡선** | 높음 | 낮음 | 중간 | 중간 | 낮음 | 중간 |
| **런타임 크기** | 큼 | 작음 | 중간 | 큼 | 작음 | 작음 |
| **복잡한 UI** | ✅ | ⚠️ | ✅ | ✅ | ⚠️ | ✅ |
| **문서화** | ✅ | ✅ | ⚠️ | ✅ | ⚠️ | ⚠️ |
| **의료 영상 사례** | ✅ 다수 | ⚠️ 일부 | ⚠️ 일부 | ❌ | ❌ | ❌ |
| **활발한 개발** | ✅ | ✅ | ✅ | ✅ | ⚠️ | ✅ |

### 3.2 기술 사양

| 프레임워크 | 언어 | 최소 C++ | 바이너리 크기 | 의존성 |
|-----------|------|---------|-------------|-------|
| Qt6 | C++ | C++17 | ~50MB+ | 다수 |
| Dear ImGui | C++ | C++11 | ~200KB | 없음 (헤더 온리) |
| wxWidgets | C++ | C++11 | ~5MB | 플랫폼 라이브러리 |
| GTK4 | C/C++ | C++17 | ~20MB | GLib, Cairo 등 |
| FLTK | C++ | C++11 | ~500KB | OpenGL |
| Slint | C++/Rust | C++20 | ~2MB | 최소 |

---

## 4. 의료 영상 요구사항 분석

### 4.1 필수 요구사항

```
┌─────────────────────────────────────────────────────────────────┐
│           Medical Imaging Viewer Requirements                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  필수 기능:                                                      │
│  ──────────                                                      │
│  1. OpenGL 4.1+ 렌더링 컨텍스트                                 │
│  2. VTK RenderWindow 임베딩                                     │
│  3. 다중 뷰포트 지원 (MPR 4-panel)                              │
│  4. 마우스/키보드 이벤트 처리                                   │
│  5. Window/Level 드래그 상호작용                                │
│  6. 메뉴/툴바/상태바                                            │
│  7. 모달/논모달 다이얼로그                                      │
│  8. 트리뷰 (Patient Browser)                                    │
│  9. 도킹 패널                                                   │
│  10. Undo/Redo 지원                                             │
│                                                                 │
│  추가 기능:                                                      │
│  ──────────                                                      │
│  • 터치스크린 지원                                               │
│  • High DPI 스케일링                                             │
│  • 다크 모드                                                     │
│  • 국제화 (i18n)                                                 │
│  • 접근성 (A11y)                                                 │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 4.2 프레임워크 적합도 점수

```
┌─────────────────────────────────────────────────────────────────┐
│              Framework Suitability for DICOM Viewer              │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Qt6        [████████████████████] 95% - 최적                   │
│             VTK 공식 지원, 완전한 위젯 세트                       │
│                                                                 │
│  Dear ImGui [████████████░░░░░░░░] 60% - 도구 UI 전용           │
│             파라미터 패널에 적합, 메인 UI 부적합                  │
│                                                                 │
│  wxWidgets  [█████████████░░░░░░░] 65% - 대안 가능              │
│             가능하나 통합 작업 추가 필요                          │
│                                                                 │
│  FLTK       [████████░░░░░░░░░░░░] 40% - 단순 뷰어만            │
│             복잡한 의료 뷰어에 제한적                             │
│                                                                 │
│  GTK4       [██████░░░░░░░░░░░░░░] 30% - Linux 전용 시 고려     │
│             VTK 통합 부실, 다른 플랫폼 비네이티브                 │
│                                                                 │
│  Slint      [████░░░░░░░░░░░░░░░░] 20% - 비권장                 │
│             VTK 통합 미검증                                      │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 5. 하이브리드 아키텍처 접근법

### 5.1 Qt + Dear ImGui 조합

두 프레임워크의 장점을 결합한 실용적 접근법:

```
┌─────────────────────────────────────────────────────────────────┐
│                   Hybrid Architecture                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   ┌───────────────────────────────────────────────────────────┐│
│   │  Qt6 (Main Window, Menus, Dialogs, Patient Browser)       ││
│   └───────────────────────────────────────────────────────────┘│
│                              │                                   │
│   ┌──────────────────────────┴──────────────────────────────┐  │
│   │               QVTKOpenGLNativeWidget                     │  │
│   │  ┌─────────────────────────────────────────────────────┐ │  │
│   │  │  VTK Render Window                                  │ │  │
│   │  │  ┌─────────────────────────────────────────────────┐│ │  │
│   │  │  │  Dear ImGui Overlay (선택적)                    ││ │  │
│   │  │  │  - Volume Controls                              ││ │  │
│   │  │  │  - Transfer Function Editor                     ││ │  │
│   │  │  │  - Real-time Parameters                         ││ │  │
│   │  │  │  - Debug Information                            ││ │  │
│   │  │  └─────────────────────────────────────────────────┘│ │  │
│   │  └─────────────────────────────────────────────────────┘ │  │
│   └──────────────────────────────────────────────────────────┘  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**구현 예시:**
```cpp
// 하이브리드 Qt + ImGui 접근법
class HybridVTKWidget : public QVTKOpenGLNativeWidget {
    Q_OBJECT
public:
    HybridVTKWidget(QWidget* parent = nullptr)
        : QVTKOpenGLNativeWidget(parent)
    {
        // VTK의 OpenGL 컨텍스트에서 ImGui 초기화
        connect(this, &QVTKOpenGLNativeWidget::frameSwapped,
                this, &HybridVTKWidget::renderImGui);
    }

protected:
    void initializeGL() override {
        QVTKOpenGLNativeWidget::initializeGL();

        // ImGui 초기화
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui_ImplOpenGL3_Init("#version 410");
    }

    void renderImGui() {
        if (!m_showImGuiOverlay) return;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();

        // ImGui 패널 렌더링
        if (m_showVolumeControls) {
            ImGui::Begin("Volume Controls", &m_showVolumeControls);
            ImGui::SliderFloat("Opacity", &m_opacity, 0.0f, 1.0f);
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

private:
    bool m_showImGuiOverlay = true;
    bool m_showVolumeControls = true;
    float m_opacity = 1.0f;
};
```

---

## 6. 시나리오별 권장사항

### 6.1 결정 매트릭스

| 시나리오 | 권장 | 대안 | 이유 |
|----------|------|------|------|
| **상용 의료 소프트웨어** | Qt6 | wxWidgets | VTK 공식 지원, FDA 인증 사례 |
| **연구용 프로토타입** | Dear ImGui | Qt6 | 빠른 개발, 파라미터 튜닝 |
| **라이선스 비용 회피** | wxWidgets | FLTK | 정적 링킹 가능 LGPL |
| **임베디드/경량** | FLTK | Dear ImGui | 최소 의존성 |
| **Linux 전용 배포** | GTK4 | Qt6 | 네이티브 GNOME 통합 |
| **현대적 선언적 UI** | Qt6 (QML) | Slint | 성숙한 생태계 |

### 6.2 DICOM Viewer 최종 권장사항

```
┌─────────────────────────────────────────────────────────────────┐
│                    권장사항 요약                                  │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  1차 선택: Qt6                                                   │
│  ────────────                                                    │
│  • VTK 공식 지원 (QVTKOpenGLNativeWidget)                       │
│  • 의료 영상 분야 검증 (3D Slicer, MITK, ParaView)              │
│  • 복잡한 애플리케이션용 완전한 기능 세트                        │
│  • 강력한 문서화와 커뮤니티                                      │
│  • 장기 유지보수 보장                                            │
│                                                                 │
│  2차 옵션: Dear ImGui (프로토타이핑 단계)                        │
│  ─────────────────────────────────────                          │
│  • 빠른 프로토타이핑과 검증                                      │
│  • 파라미터 튜닝 인터페이스                                      │
│  • 나중에 Qt로 마이그레이션 가능                                 │
│  • MIT 라이선스 - 무료                                           │
│                                                                 │
│  하이브리드 접근법: Qt6 + ImGui 오버레이                         │
│  ─────────────────────────────────────                          │
│  • 메인 애플리케이션 구조에 Qt                                   │
│  • 실시간 컨트롤 패널에 ImGui                                    │
│  • 두 프레임워크의 장점 결합                                     │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 7. 마이그레이션 고려사항

### 7.1 Qt에서 대안으로 전환

Qt에서 다른 프레임워크로 마이그레이션이 필요한 경우 (예: 라이선스 비용):

1. **VTK 레이어 추상화 우선**: GUI 독립적 VTK 렌더링 코어 생성
2. **인터페이스 레이어 정의**: GUI 작업 추상화 (메뉴, 다이얼로그, 이벤트)
3. **점진적 마이그레이션**: Qt 위젯을 단계적으로 교체
4. **테스트 전략**: 포괄적 테스트로 기능 동등성 유지

### 7.2 코드 추상화 예시

```cpp
// 향후 이식성을 위한 GUI 독립적 인터페이스
class IViewerWindow {
public:
    virtual ~IViewerWindow() = default;

    // 핵심 기능
    virtual void SetVolumeData(vtkImageData* data) = 0;
    virtual void SetWindowLevel(double window, double level) = 0;
    virtual void ResetCamera() = 0;

    // UI 기능
    virtual void ShowMessage(const std::string& message) = 0;
    virtual std::string ShowFileDialog(const std::string& filter) = 0;
    virtual void UpdateStatusBar(const std::string& status) = 0;
};

// Qt 구현
class QtViewerWindow : public QMainWindow, public IViewerWindow {
    // Qt 전용 구현
};

// wxWidgets 구현 (향후)
class WxViewerWindow : public wxFrame, public IViewerWindow {
    // wxWidgets 전용 구현
};
```

---

## 8. 참고 자료

### 공식 문서
- [Qt Documentation](https://doc.qt.io/)
- [Dear ImGui Repository](https://github.com/ocornut/imgui)
- [wxWidgets Documentation](https://docs.wxwidgets.org/)
- [GTK Documentation](https://docs.gtk.org/)
- [FLTK Documentation](https://www.fltk.org/doc-1.4/)
- [Slint Documentation](https://slint.dev/docs/)

### VTK 통합 리소스
- [VTK Qt Integration](https://vtk.org/doc/nightly/html/group__QtOpenGL.html)
- [VTK Examples - Qt](https://examples.vtk.org/site/Cxx/#qt)

### 의료 영상 레퍼런스
- [3D Slicer (Qt 기반)](https://www.slicer.org/)
- [MITK (Qt 기반)](https://www.mitk.org/)
- [ParaView (Qt 기반)](https://www.paraview.org/)

---

*이전 문서: [05-pacs-integration.md](05-pacs-integration.md) - pacs_system 연동 가이드*
*인덱스로 돌아가기: [README.md](README.md) - 문서 인덱스*

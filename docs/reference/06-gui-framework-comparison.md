# C++ GUI Framework Comparison for Medical Imaging

> **Last Updated**: 2025-12-31
> **Context**: DICOM Viewer GUI Framework Selection
> **Current Choice**: Qt6 (Recommended)

## 1. Overview

### 1.1 GUI Framework Landscape

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
│   VTK Integration:  Qt ★★★★★  |  ImGui ★★★★☆  |  Others ★★☆☆☆        │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 1.2 Retained Mode vs Immediate Mode

| Aspect | Retained Mode | Immediate Mode |
|--------|---------------|----------------|
| **Concept** | Widgets maintain state | UI rebuilt every frame |
| **Examples** | Qt, wxWidgets, GTK | Dear ImGui, Nuklear |
| **Memory** | Higher (widget objects) | Lower (no persistent state) |
| **Complexity** | Complex applications | Simple/tool UIs |
| **Performance** | Event-driven | Frame-driven (60+ FPS) |
| **Learning Curve** | Higher | Lower |

---

## 2. Framework Detailed Comparison

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
│  Strengths:                         Weaknesses:                  │
│  ──────────                         ───────────                  │
│  ✓ Official VTK support             ✗ Large runtime (~50MB+)    │
│  ✓ Comprehensive widget set         ✗ Complex build system      │
│  ✓ Excellent documentation          ✗ Commercial license cost   │
│  ✓ Cross-platform consistency       ✗ Steep learning curve      │
│  ✓ Strong IDE support (Qt Creator)  ✗ MOC preprocessing         │
│  ✓ Mature ecosystem (20+ years)     ✗ Heavy dependency          │
│  ✓ Medical imaging references       │                           │
│    (3D Slicer, MITK, ParaView)      │                           │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**VTK Integration:**
```cpp
// Qt6 + VTK: Official Integration
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
        // Volume rendering setup
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

**Key Features for Medical Imaging:**
- `QVTKOpenGLNativeWidget`: Native VTK widget integration
- `QVTKOpenGLStereoWidget`: Stereo 3D support
- Signal/Slot for event handling
- QML for modern declarative UI

---

### 2.2 Dear ImGui

```
┌─────────────────────────────────────────────────────────────────┐
│                         Dear ImGui                               │
├─────────────────────────────────────────────────────────────────┤
│  Type: Immediate Mode GUI                                        │
│  License: MIT (Very permissive for commercial use)               │
│  Repository: github.com/ocornut/imgui                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Strengths:                         Weaknesses:                  │
│  ──────────                         ───────────                  │
│  ✓ Very lightweight (~15K LOC)      ✗ No native look and feel   │
│  ✓ Header-only integration          ✗ Complex layouts difficult │
│  ✓ Easy VTK integration             ✗ Limited text editing      │
│  ✓ Optimal for real-time tuning     ✗ No accessibility (A11y)   │
│  ✓ Rapid prototyping                ✗ Manual i18n implementation│
│  ✓ GPU rendering based              ✗ No file dialogs built-in  │
│  ✓ MIT license (free commercial)    ✗ Not suitable for complex  │
│                                       application UIs            │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**VTK Integration:**
```cpp
// Dear ImGui + VTK Integration
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

        // VTK setup
        m_renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
        m_renderWindow->SetWindowId(window);  // Share OpenGL context
    }

    void Render() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ImGui UI
        RenderControlPanel();

        // VTK Rendering (same OpenGL context)
        m_renderWindow->Render();

        // ImGui Rendering (overlay)
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
        ImGui::Text("Presets:");
        if (ImGui::Button("CT Bone")) { SetPreset(Preset::CTBone); }
        ImGui::SameLine();
        if (ImGui::Button("CT Soft Tissue")) { SetPreset(Preset::CTSoftTissue); }

        ImGui::Separator();
        if (ImGui::CollapsingHeader("Transfer Function")) {
            // Transfer function editor
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

**Best Use Cases:**
- Research prototypes
- Parameter tuning interfaces
- Debug/development overlays
- Real-time visualization tools

---

### 2.3 wxWidgets

```
┌─────────────────────────────────────────────────────────────────┐
│                         wxWidgets                                │
├─────────────────────────────────────────────────────────────────┤
│  Type: Retained Mode GUI (Native Wrapper)                        │
│  License: wxWindows License (LGPL compatible, commercial-friendly)│
│  Repository: github.com/wxWidgets/wxWidgets                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Strengths:                         Weaknesses:                  │
│  ──────────                         ───────────                  │
│  ✓ True native look and feel        ✗ VTK integration complex   │
│  ✓ Commercial-friendly license      ✗ Less documentation than Qt│
│  ✓ Mature library (30+ years)       ✗ Smaller community         │
│  ✓ Cross-platform                   ✗ Limited modern C++ support│
│  ✓ Lightweight runtime              ✗ Manual OpenGL widget impl │
│  ✓ No commercial license cost       ✗ Fewer medical imaging refs│
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**VTK Integration:**
```cpp
// wxWidgets + VTK Integration
#include <wx/wx.h>
#include <wx/glcanvas.h>
#include "wxVTKRenderWindowInteractor.h"
#include <vtkRenderer.h>

class VTKFrame : public wxFrame {
public:
    VTKFrame() : wxFrame(nullptr, wxID_ANY, "DICOM Viewer",
                         wxDefaultPosition, wxSize(1200, 800))
    {
        // Create VTK widget
        m_vtkWidget = new wxVTKRenderWindowInteractor(this, wxID_ANY);

        // Setup renderer
        m_renderer = vtkSmartPointer<vtkRenderer>::New();
        m_vtkWidget->GetRenderWindow()->AddRenderer(m_renderer);

        // Layout
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(m_vtkWidget, 1, wxEXPAND);
        SetSizer(sizer);

        // Create menus
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
        wxDirDialog dialog(this, "Select DICOM Directory");
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
│  Strengths:                         Weaknesses:                  │
│  ──────────                         ───────────                  │
│  ✓ Native on Linux/GNOME            ✗ Non-native on Windows/Mac │
│  ✓ Modern UI design (GTK4)          ✗ No VTK integration examples│
│  ✓ Active development               ✗ Cross-platform inconsistent│
│  ✓ CSS styling support              ✗ Few medical imaging cases │
│  ✓ Good accessibility               ✗ Complex C++ bindings      │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**VTK Integration (Custom):**
```cpp
// GTK4 + VTK requires custom GLArea integration
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
        // Custom OpenGL context sharing required
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
│  Strengths:                         Weaknesses:                  │
│  ──────────                         ───────────                  │
│  ✓ Very lightweight (~500KB)        ✗ Dated UI design           │
│  ✓ Fast compilation                 ✗ Limited modern widgets    │
│  ✓ Easy OpenGL integration          ✗ Poor documentation        │
│  ✓ Static linking allowed           ✗ Complex layouts difficult │
│  ✓ Minimal dependencies             ✗ Inactive development      │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**VTK Integration:**
```cpp
// FLTK + VTK Integration
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Gl_Window.H>
#include <vtkRenderWindow.h>

class VTKFltkWindow : public Fl_Gl_Window {
public:
    VTKFltkWindow(int x, int y, int w, int h)
        : Fl_Gl_Window(x, y, w, h)
    {
        mode(FL_RGB | FL_DOUBLE | FL_DEPTH);
    }

    void draw() override {
        if (!valid()) {
            // Initialize VTK on first draw
            m_renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
        }
        m_renderWindow->Render();
    }

    int handle(int event) override {
        // Forward events to VTK interactor
        switch (event) {
            case FL_PUSH:
            case FL_DRAG:
            case FL_RELEASE:
                // Handle mouse events
                return 1;
        }
        return Fl_Gl_Window::handle(event);
    }

private:
    vtkSmartPointer<vtkRenderWindow> m_renderWindow;
};
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
│  Strengths:                         Weaknesses:                  │
│  ──────────                         ───────────                  │
│  ✓ Modern declarative UI            ✗ Commercial license cost   │
│  ✓ Rust/C++/JavaScript bindings     ✗ Early stage (2021~)       │
│  ✓ Live preview                     ✗ No VTK integration cases  │
│  ✓ Embedded support                 ✗ Small community/ecosystem │
│  ✓ Small runtime                    ✗ Limited widget set        │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. Comprehensive Comparison Matrix

### 3.1 Feature Comparison

| Criterion | Qt6 | Dear ImGui | wxWidgets | GTK4 | FLTK | Slint |
|-----------|:---:|:---:|:---:|:---:|:---:|:---:|
| **Official VTK Support** | ✅ | ⚠️ | ⚠️ | ❌ | ⚠️ | ❌ |
| **Cross-Platform** | ✅ | ✅ | ✅ | ⚠️ | ✅ | ✅ |
| **Native Look** | ⚠️ | ❌ | ✅ | ⚠️ | ❌ | ❌ |
| **License** | LGPL/Comm | MIT | LGPL+ | LGPL | LGPL+ | GPL/Comm |
| **Learning Curve** | High | Low | Medium | Medium | Low | Medium |
| **Runtime Size** | Large | Small | Medium | Large | Small | Small |
| **Complex UI** | ✅ | ⚠️ | ✅ | ✅ | ⚠️ | ✅ |
| **Documentation** | ✅ | ✅ | ⚠️ | ✅ | ⚠️ | ⚠️ |
| **Medical Imaging Cases** | ✅ Many | ⚠️ Some | ⚠️ Some | ❌ | ❌ | ❌ |
| **Active Development** | ✅ | ✅ | ✅ | ✅ | ⚠️ | ✅ |

### 3.2 Technical Specifications

| Framework | Language | Min C++ | Binary Size | Dependencies |
|-----------|----------|---------|-------------|--------------|
| Qt6 | C++ | C++17 | ~50MB+ | Many |
| Dear ImGui | C++ | C++11 | ~200KB | None (header-only) |
| wxWidgets | C++ | C++11 | ~5MB | Platform libs |
| GTK4 | C/C++ | C++17 | ~20MB | GLib, Cairo, etc. |
| FLTK | C++ | C++11 | ~500KB | OpenGL |
| Slint | C++/Rust | C++20 | ~2MB | Minimal |

---

## 4. Medical Imaging Requirements Analysis

### 4.1 Essential Requirements

```
┌─────────────────────────────────────────────────────────────────┐
│           Medical Imaging Viewer Requirements                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Must-Have Features:                                             │
│  ───────────────────                                             │
│  1. OpenGL 4.1+ rendering context                               │
│  2. VTK RenderWindow embedding                                  │
│  3. Multi-viewport support (MPR 4-panel)                        │
│  4. Mouse/keyboard event handling                               │
│  5. Window/Level drag interaction                               │
│  6. Menu/Toolbar/Statusbar                                      │
│  7. Modal/Non-modal dialogs                                     │
│  8. Tree view (Patient Browser)                                 │
│  9. Dockable panels                                             │
│  10. Undo/Redo support                                          │
│                                                                 │
│  Nice-to-Have Features:                                          │
│  ─────────────────────                                           │
│  • Touch screen support                                          │
│  • High DPI scaling                                              │
│  • Dark mode                                                     │
│  • Internationalization (i18n)                                   │
│  • Accessibility (A11y)                                          │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 4.2 Framework Suitability Score

```
┌─────────────────────────────────────────────────────────────────┐
│              Framework Suitability for DICOM Viewer              │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Qt6        [████████████████████] 95% - Optimal                │
│             Official VTK support, complete widget set           │
│                                                                 │
│  Dear ImGui [████████████░░░░░░░░] 60% - Tool UI only           │
│             Great for parameter panels, not main UI             │
│                                                                 │
│  wxWidgets  [█████████████░░░░░░░] 65% - Viable alternative     │
│             Possible but requires more integration work         │
│                                                                 │
│  FLTK       [████████░░░░░░░░░░░░] 40% - Simple viewers only    │
│             Too limited for complex medical viewers             │
│                                                                 │
│  GTK4       [██████░░░░░░░░░░░░░░] 30% - Linux only consider    │
│             Poor VTK integration, non-native elsewhere          │
│                                                                 │
│  Slint      [████░░░░░░░░░░░░░░░░] 20% - Not recommended        │
│             VTK integration unproven                            │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 5. Hybrid Architecture Approach

### 5.1 Qt + Dear ImGui Combination

A practical approach combining the strengths of both frameworks:

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
│   │  │  │  Dear ImGui Overlay (Optional)                  ││ │  │
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

**Implementation Example:**
```cpp
// Hybrid Qt + ImGui approach
class HybridVTKWidget : public QVTKOpenGLNativeWidget {
    Q_OBJECT
public:
    HybridVTKWidget(QWidget* parent = nullptr)
        : QVTKOpenGLNativeWidget(parent)
    {
        // Initialize ImGui in VTK's OpenGL context
        connect(this, &QVTKOpenGLNativeWidget::frameSwapped,
                this, &HybridVTKWidget::renderImGui);
    }

protected:
    void initializeGL() override {
        QVTKOpenGLNativeWidget::initializeGL();

        // Initialize ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui_ImplOpenGL3_Init("#version 410");
    }

    void renderImGui() {
        if (!m_showImGuiOverlay) return;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();

        // Render ImGui panels
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

## 6. Recommendations by Scenario

### 6.1 Decision Matrix

| Scenario | Recommended | Alternative | Rationale |
|----------|-------------|-------------|-----------|
| **Commercial Medical Software** | Qt6 | wxWidgets | VTK official support, FDA references |
| **Research Prototype** | Dear ImGui | Qt6 | Fast development, parameter tuning |
| **License Cost Avoidance** | wxWidgets | FLTK | LGPL with static linking |
| **Embedded/Lightweight** | FLTK | Dear ImGui | Minimal dependencies |
| **Linux-Only Deployment** | GTK4 | Qt6 | Native GNOME integration |
| **Modern Declarative UI** | Qt6 (QML) | Slint | Mature ecosystem |

### 6.2 Final Recommendation for DICOM Viewer

```
┌─────────────────────────────────────────────────────────────────┐
│                    RECOMMENDATION SUMMARY                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  PRIMARY CHOICE: Qt6                                             │
│  ────────────────────                                            │
│  • Official VTK support (QVTKOpenGLNativeWidget)                │
│  • Proven in medical imaging (3D Slicer, MITK, ParaView)        │
│  • Complete feature set for complex applications                │
│  • Strong documentation and community                           │
│  • Long-term maintenance guaranteed                             │
│                                                                 │
│  SECONDARY OPTION: Dear ImGui (for prototyping phase)           │
│  ─────────────────────────────────────────────────              │
│  • Rapid prototyping and validation                             │
│  • Parameter tuning interfaces                                   │
│  • Can be migrated to Qt later                                  │
│  • MIT license - no cost                                        │
│                                                                 │
│  HYBRID APPROACH: Qt6 + ImGui Overlay                           │
│  ───────────────────────────────────                            │
│  • Qt for main application structure                            │
│  • ImGui for real-time control panels                           │
│  • Best of both worlds                                          │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 7. Migration Considerations

### 7.1 From Qt to Alternatives

If migrating away from Qt is required (e.g., license cost):

1. **Abstract VTK Layer First**: Create a GUI-agnostic VTK rendering core
2. **Define Interface Layer**: Abstract GUI operations (menus, dialogs, events)
3. **Gradual Migration**: Replace Qt widgets incrementally
4. **Testing Strategy**: Maintain feature parity through comprehensive tests

### 7.2 Code Abstraction Example

```cpp
// GUI-agnostic interface for future portability
class IViewerWindow {
public:
    virtual ~IViewerWindow() = default;

    // Core operations
    virtual void SetVolumeData(vtkImageData* data) = 0;
    virtual void SetWindowLevel(double window, double level) = 0;
    virtual void ResetCamera() = 0;

    // UI operations
    virtual void ShowMessage(const std::string& message) = 0;
    virtual std::string ShowFileDialog(const std::string& filter) = 0;
    virtual void UpdateStatusBar(const std::string& status) = 0;
};

// Qt implementation
class QtViewerWindow : public QMainWindow, public IViewerWindow {
    // Qt-specific implementation
};

// wxWidgets implementation (future)
class WxViewerWindow : public wxFrame, public IViewerWindow {
    // wxWidgets-specific implementation
};
```

---

## 8. References

### Official Documentation
- [Qt Documentation](https://doc.qt.io/)
- [Dear ImGui Repository](https://github.com/ocornut/imgui)
- [wxWidgets Documentation](https://docs.wxwidgets.org/)
- [GTK Documentation](https://docs.gtk.org/)
- [FLTK Documentation](https://www.fltk.org/doc-1.4/)
- [Slint Documentation](https://slint.dev/docs/)

### VTK Integration Resources
- [VTK Qt Integration](https://vtk.org/doc/nightly/html/group__QtOpenGL.html)
- [VTK Examples - Qt](https://examples.vtk.org/site/Cxx/#qt)

### Medical Imaging References
- [3D Slicer (Qt-based)](https://www.slicer.org/)
- [MITK (Qt-based)](https://www.mitk.org/)
- [ParaView (Qt-based)](https://www.paraview.org/)

---

*Previous Document: [05-pacs-integration.md](05-pacs-integration.md) - pacs_system Integration Guide*
*Back to Index: [README.md](README.md) - Document Index*

# Remote Visualization Architecture

> **Last Updated**: 2025-12-31
> **Architecture Pattern**: Server-Side Rendering + Image Streaming
> **Use Case**: Platform-Independent Medical Image Viewing

## 1. Overview

### 1.1 Architecture Concept

Remote visualization enables platform-independent medical image viewing by rendering on a server and streaming images to any client device. This approach is proven in production systems like ParaView Web, 3D Slicer Web, and enterprise PACS solutions.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                 Remote Visualization Architecture                        │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Client (Any Platform)              Server (VTK Backend)               │
│   ─────────────────────              ────────────────────               │
│                                                                         │
│   ┌─────────────────┐               ┌─────────────────────────────────┐│
│   │  Web Browser    │               │        VTK Render Server        ││
│   │  iOS App        │   WebSocket   │  ┌───────────────────────────┐  ││
│   │  Android App    │◄─────────────►│  │  vtkRenderWindow          │  ││
│   │  Desktop App    │   (Events)    │  │  (Offscreen Rendering)    │  ││
│   │  Smart TV       │               │  └───────────────────────────┘  ││
│   └─────────────────┘               │              │                  ││
│          │                          │              ▼                  ││
│          │                          │  ┌───────────────────────────┐  ││
│          │      Image Stream        │  │  Image Encoder            │  ││
│          │◄─────────────────────────│  │  (JPEG/PNG/WebP/H.264)    │  ││
│          │      (JPEG/H.264)        │  └───────────────────────────┘  ││
│          │                          │                                 ││
│   ┌─────────────────┐               │  ┌───────────────────────────┐  ││
│   │  <canvas> or    │               │  │  ITK Processing           │  ││
│   │  <video> or     │               │  │  pacs_system DICOM I/O    │  ││
│   │  Native View    │               │  └───────────────────────────┘  ││
│   └─────────────────┘               └─────────────────────────────────┘│
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 1.2 Key Benefits

| Benefit | Description |
|---------|-------------|
| **Platform Independence** | Web, iOS, Android, Desktop - single codebase |
| **No Client GPU Required** | Rendering on server, display on any device |
| **Large Data Handling** | Multi-GB volumes without client download |
| **Security/Compliance** | PHI stays on server (HIPAA/GDPR friendly) |
| **Centralized Management** | Server updates apply to all clients |
| **Remote Collaboration** | Multi-user viewing, telemedicine support |

---

## 2. Streaming Methods Comparison

### 2.1 Image Streaming Options

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Streaming Method Comparison                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   JPEG Sequence          WebP Sequence          H.264/H.265 Video       │
│   ──────────────         ─────────────          ─────────────────       │
│   ┌────────────┐         ┌────────────┐         ┌────────────┐         │
│   │  Frame 1   │         │  Frame 1   │         │ I-Frame    │         │
│   │  (JPEG)    │         │  (WebP)    │         │ P-Frame    │         │
│   │  ~50KB     │         │  ~30KB     │         │ P-Frame    │         │
│   ├────────────┤         ├────────────┤         │ P-Frame    │         │
│   │  Frame 2   │         │  Frame 2   │         │ ...        │         │
│   │  (JPEG)    │         │  (WebP)    │         │ ~5KB avg   │         │
│   │  ~50KB     │         │  ~30KB     │         └────────────┘         │
│   └────────────┘         └────────────┘                                 │
│                                                                         │
│   Latency: Medium        Latency: Medium        Latency: Low            │
│   Quality: Medium        Quality: High          Quality: High           │
│   Bandwidth: High        Bandwidth: Medium      Bandwidth: Low          │
│   Complexity: Low        Complexity: Low        Complexity: High        │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Detailed Comparison

| Method | Latency | Quality | Bandwidth | Complexity | Best For |
|--------|---------|---------|-----------|------------|----------|
| **JPEG Sequence** | ~100ms | Medium | ~3 MB/s @30fps | Low | Simple implementation |
| **PNG Sequence** | ~150ms | Lossless | ~10 MB/s @30fps | Low | Diagnostic accuracy |
| **WebP Sequence** | ~80ms | High | ~1.5 MB/s @30fps | Low | Balanced choice |
| **H.264 Stream** | ~50ms | High | ~500 KB/s @30fps | High | Real-time interaction |
| **H.265/HEVC** | ~50ms | Very High | ~300 KB/s @30fps | Very High | 4K streaming |
| **WebRTC** | ~30ms | High | Variable | High | Collaboration/Telemedicine |

---

## 3. Implementation Architecture

### 3.1 System Components

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      System Component Diagram                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                        Load Balancer                             │   │
│  │                     (nginx / HAProxy)                            │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                │                                        │
│           ┌───────────────────┼───────────────────┐                    │
│           ▼                   ▼                   ▼                     │
│  ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐          │
│  │  Render Node 1  │ │  Render Node 2  │ │  Render Node N  │          │
│  │  ┌───────────┐  │ │  ┌───────────┐  │ │  ┌───────────┐  │          │
│  │  │    GPU    │  │ │  │    GPU    │  │ │  │    GPU    │  │          │
│  │  │  (NVIDIA) │  │ │  │  (NVIDIA) │  │ │  │  (NVIDIA) │  │          │
│  │  └───────────┘  │ │  └───────────┘  │ │  └───────────┘  │          │
│  │  ┌───────────┐  │ │  ┌───────────┐  │ │  ┌───────────┐  │          │
│  │  │    VTK    │  │ │  │    VTK    │  │ │  │    VTK    │  │          │
│  │  │  Render   │  │ │  │  Render   │  │ │  │  Render   │  │          │
│  │  └───────────┘  │ │  └───────────┘  │ │  └───────────┘  │          │
│  │  ┌───────────┐  │ │  ┌───────────┐  │ │  ┌───────────┐  │          │
│  │  │  Encoder  │  │ │  │  Encoder  │  │ │  │  Encoder  │  │          │
│  │  │(NVENC/x264)│ │ │  │(NVENC/x264)│ │ │  │(NVENC/x264)│ │          │
│  │  └───────────┘  │ │  └───────────┘  │ │  └───────────┘  │          │
│  └─────────────────┘ └─────────────────┘ └─────────────────┘          │
│           │                   │                   │                     │
│           └───────────────────┼───────────────────┘                    │
│                               ▼                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                     Session Manager                              │   │
│  │              (Redis / In-Memory State Store)                     │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                               │                                         │
│                               ▼                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                      DICOM Storage                               │   │
│  │               (pacs_system / File System / S3)                   │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 3.2 Data Flow

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         Data Flow Sequence                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Client                    Server                     Storage           │
│    │                         │                           │              │
│    │  1. Connect (WS)        │                           │              │
│    │────────────────────────►│                           │              │
│    │                         │                           │              │
│    │  2. Load Study Request  │                           │              │
│    │────────────────────────►│  3. Fetch DICOM          │              │
│    │                         │──────────────────────────►│              │
│    │                         │◄──────────────────────────│              │
│    │                         │  4. Parse & Process (ITK) │              │
│    │                         │                           │              │
│    │  5. Initial Frame       │                           │              │
│    │◄────────────────────────│                           │              │
│    │     (Full Quality)      │                           │              │
│    │                         │                           │              │
│    │  6. Mouse Drag Start    │                           │              │
│    │────────────────────────►│                           │              │
│    │  7. Low-Res Frame       │                           │              │
│    │◄────────────────────────│  (Fast response)         │              │
│    │  8. Mouse Move          │                           │              │
│    │────────────────────────►│                           │              │
│    │  9. Low-Res Frame       │                           │              │
│    │◄────────────────────────│                           │              │
│    │  10. Mouse Release      │                           │              │
│    │────────────────────────►│                           │              │
│    │  11. High-Res Frame     │                           │              │
│    │◄────────────────────────│  (Progressive refine)    │              │
│    │                         │                           │              │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Server Implementation

### 4.1 VTK Render Server Core

```cpp
// render_server.hpp
#pragma once

#include <vtkSmartPointer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkVolume.h>
#include <vtkVolumeProperty.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>
#include <vtkWindowToImageFilter.h>
#include <vtkJPEGWriter.h>
#include <vtkPNGWriter.h>
#include <vtkImageData.h>
#include <vtkCamera.h>

#include <vector>
#include <memory>
#include <mutex>

namespace dicom_viewer {

struct RenderSettings {
    int width = 1920;
    int height = 1080;
    int jpegQuality = 85;
    bool useGPU = true;
};

struct InteractionState {
    double cameraPosition[3];
    double cameraFocalPoint[3];
    double cameraViewUp[3];
    double windowWidth = 400.0;
    double windowLevel = 40.0;
    double zoom = 1.0;
};

class RenderServer {
public:
    RenderServer(const RenderSettings& settings = {});
    ~RenderServer();

    // Initialization
    bool Initialize();
    void SetImageData(vtkImageData* imageData);

    // Rendering
    std::vector<uint8_t> RenderToJPEG(int quality = -1);
    std::vector<uint8_t> RenderToPNG();
    std::vector<uint8_t> RenderToWebP(int quality = 80);

    // Interaction
    void SetWindowLevel(double window, double level);
    void RotateCamera(double deltaAzimuth, double deltaElevation);
    void ZoomCamera(double factor);
    void PanCamera(double deltaX, double deltaY);
    void ResetCamera();

    // State management
    InteractionState GetState() const;
    void SetState(const InteractionState& state);

    // Presets
    void ApplyPreset(const std::string& presetName);

private:
    void SetupVolumeRendering();
    void SetupTransferFunction(const std::string& preset);
    void UpdateWindowLevel();

    RenderSettings m_settings;
    InteractionState m_state;
    std::mutex m_renderMutex;

    vtkSmartPointer<vtkRenderWindow> m_renderWindow;
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkGPUVolumeRayCastMapper> m_volumeMapper;
    vtkSmartPointer<vtkVolume> m_volume;
    vtkSmartPointer<vtkVolumeProperty> m_volumeProperty;
    vtkSmartPointer<vtkColorTransferFunction> m_colorTF;
    vtkSmartPointer<vtkPiecewiseFunction> m_opacityTF;
    vtkSmartPointer<vtkWindowToImageFilter> m_windowToImage;
};

} // namespace dicom_viewer
```

```cpp
// render_server.cpp
#include "render_server.hpp"

namespace dicom_viewer {

RenderServer::RenderServer(const RenderSettings& settings)
    : m_settings(settings)
{
}

RenderServer::~RenderServer() = default;

bool RenderServer::Initialize()
{
    // Create offscreen render window
    m_renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
    m_renderWindow->SetOffScreenRendering(1);
    m_renderWindow->SetSize(m_settings.width, m_settings.height);
    m_renderWindow->SetMultiSamples(0);  // Disable AA for performance

    // Create renderer
    m_renderer = vtkSmartPointer<vtkRenderer>::New();
    m_renderer->SetBackground(0.0, 0.0, 0.0);
    m_renderWindow->AddRenderer(m_renderer);

    // Setup volume rendering pipeline
    SetupVolumeRendering();

    // Setup window-to-image filter
    m_windowToImage = vtkSmartPointer<vtkWindowToImageFilter>::New();
    m_windowToImage->SetInput(m_renderWindow);
    m_windowToImage->SetInputBufferTypeToRGB();
    m_windowToImage->ReadFrontBufferOff();

    return true;
}

void RenderServer::SetupVolumeRendering()
{
    m_volumeMapper = vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
    m_volumeMapper->SetAutoAdjustSampleDistances(1);
    m_volumeMapper->SetSampleDistance(0.5);

    m_volumeProperty = vtkSmartPointer<vtkVolumeProperty>::New();
    m_volumeProperty->SetInterpolationTypeToLinear();
    m_volumeProperty->ShadeOn();
    m_volumeProperty->SetAmbient(0.2);
    m_volumeProperty->SetDiffuse(0.7);
    m_volumeProperty->SetSpecular(0.3);

    m_colorTF = vtkSmartPointer<vtkColorTransferFunction>::New();
    m_opacityTF = vtkSmartPointer<vtkPiecewiseFunction>::New();

    m_volumeProperty->SetColor(m_colorTF);
    m_volumeProperty->SetScalarOpacity(m_opacityTF);

    m_volume = vtkSmartPointer<vtkVolume>::New();
    m_volume->SetMapper(m_volumeMapper);
    m_volume->SetProperty(m_volumeProperty);

    m_renderer->AddVolume(m_volume);
}

void RenderServer::SetImageData(vtkImageData* imageData)
{
    std::lock_guard<std::mutex> lock(m_renderMutex);

    m_volumeMapper->SetInputData(imageData);
    m_renderer->ResetCamera();

    // Store initial camera state
    auto camera = m_renderer->GetActiveCamera();
    camera->GetPosition(m_state.cameraPosition);
    camera->GetFocalPoint(m_state.cameraFocalPoint);
    camera->GetViewUp(m_state.cameraViewUp);

    // Apply default preset
    ApplyPreset("CT_Bone");
}

std::vector<uint8_t> RenderServer::RenderToJPEG(int quality)
{
    std::lock_guard<std::mutex> lock(m_renderMutex);

    if (quality < 0) quality = m_settings.jpegQuality;

    m_renderWindow->Render();
    m_windowToImage->Modified();
    m_windowToImage->Update();

    auto jpegWriter = vtkSmartPointer<vtkJPEGWriter>::New();
    jpegWriter->SetInputConnection(m_windowToImage->GetOutputPort());
    jpegWriter->SetQuality(quality);
    jpegWriter->WriteToMemoryOn();
    jpegWriter->Write();

    auto result = jpegWriter->GetResult();
    return std::vector<uint8_t>(
        result->GetPointer(0),
        result->GetPointer(0) + result->GetNumberOfTuples()
    );
}

void RenderServer::SetWindowLevel(double window, double level)
{
    std::lock_guard<std::mutex> lock(m_renderMutex);

    m_state.windowWidth = window;
    m_state.windowLevel = level;
    UpdateWindowLevel();
}

void RenderServer::UpdateWindowLevel()
{
    double lower = m_state.windowLevel - m_state.windowWidth / 2.0;
    double upper = m_state.windowLevel + m_state.windowWidth / 2.0;

    m_opacityTF->RemoveAllPoints();
    m_opacityTF->AddPoint(lower - 1, 0.0);
    m_opacityTF->AddPoint(lower, 0.0);
    m_opacityTF->AddPoint(upper, 1.0);
    m_opacityTF->AddPoint(upper + 1, 1.0);
}

void RenderServer::RotateCamera(double deltaAzimuth, double deltaElevation)
{
    std::lock_guard<std::mutex> lock(m_renderMutex);

    auto camera = m_renderer->GetActiveCamera();
    camera->Azimuth(deltaAzimuth);
    camera->Elevation(deltaElevation);
    camera->OrthogonalizeViewUp();

    // Update state
    camera->GetPosition(m_state.cameraPosition);
    camera->GetViewUp(m_state.cameraViewUp);
}

void RenderServer::ZoomCamera(double factor)
{
    std::lock_guard<std::mutex> lock(m_renderMutex);

    auto camera = m_renderer->GetActiveCamera();
    camera->Zoom(factor);
    m_state.zoom *= factor;
}

void RenderServer::PanCamera(double deltaX, double deltaY)
{
    std::lock_guard<std::mutex> lock(m_renderMutex);

    auto camera = m_renderer->GetActiveCamera();

    double viewFocus[4], viewPoint[3];
    camera->GetFocalPoint(viewFocus);
    camera->GetPosition(viewPoint);

    camera->SetFocalPoint(
        viewFocus[0] - deltaX,
        viewFocus[1] - deltaY,
        viewFocus[2]
    );
    camera->SetPosition(
        viewPoint[0] - deltaX,
        viewPoint[1] - deltaY,
        viewPoint[2]
    );

    camera->GetFocalPoint(m_state.cameraFocalPoint);
    camera->GetPosition(m_state.cameraPosition);
}

void RenderServer::ResetCamera()
{
    std::lock_guard<std::mutex> lock(m_renderMutex);

    m_renderer->ResetCamera();
    m_state.zoom = 1.0;

    auto camera = m_renderer->GetActiveCamera();
    camera->GetPosition(m_state.cameraPosition);
    camera->GetFocalPoint(m_state.cameraFocalPoint);
    camera->GetViewUp(m_state.cameraViewUp);
}

void RenderServer::ApplyPreset(const std::string& presetName)
{
    SetupTransferFunction(presetName);
}

void RenderServer::SetupTransferFunction(const std::string& preset)
{
    m_colorTF->RemoveAllPoints();
    m_opacityTF->RemoveAllPoints();

    if (preset == "CT_Bone") {
        // Bone visualization
        m_colorTF->AddRGBPoint(-1000, 0.0, 0.0, 0.0);
        m_colorTF->AddRGBPoint(200, 0.9, 0.8, 0.7);
        m_colorTF->AddRGBPoint(500, 1.0, 1.0, 0.9);
        m_colorTF->AddRGBPoint(1500, 1.0, 1.0, 1.0);

        m_opacityTF->AddPoint(-1000, 0.0);
        m_opacityTF->AddPoint(200, 0.0);
        m_opacityTF->AddPoint(300, 0.3);
        m_opacityTF->AddPoint(1500, 0.8);
    }
    else if (preset == "CT_SoftTissue") {
        // Soft tissue
        m_colorTF->AddRGBPoint(-1000, 0.0, 0.0, 0.0);
        m_colorTF->AddRGBPoint(-100, 0.6, 0.3, 0.2);
        m_colorTF->AddRGBPoint(100, 0.9, 0.6, 0.5);
        m_colorTF->AddRGBPoint(300, 1.0, 0.9, 0.8);

        m_opacityTF->AddPoint(-1000, 0.0);
        m_opacityTF->AddPoint(-100, 0.0);
        m_opacityTF->AddPoint(0, 0.1);
        m_opacityTF->AddPoint(100, 0.3);
        m_opacityTF->AddPoint(300, 0.5);
    }
    else if (preset == "CT_Lung") {
        // Lung visualization
        m_colorTF->AddRGBPoint(-1000, 0.0, 0.0, 0.0);
        m_colorTF->AddRGBPoint(-600, 0.2, 0.4, 0.6);
        m_colorTF->AddRGBPoint(-400, 0.4, 0.6, 0.8);
        m_colorTF->AddRGBPoint(0, 0.8, 0.8, 0.8);

        m_opacityTF->AddPoint(-1000, 0.0);
        m_opacityTF->AddPoint(-900, 0.0);
        m_opacityTF->AddPoint(-600, 0.1);
        m_opacityTF->AddPoint(-400, 0.2);
        m_opacityTF->AddPoint(0, 0.0);
    }
    else if (preset == "CT_Angio") {
        // Angiography (contrast-enhanced vessels)
        m_colorTF->AddRGBPoint(-1000, 0.0, 0.0, 0.0);
        m_colorTF->AddRGBPoint(100, 0.8, 0.2, 0.1);
        m_colorTF->AddRGBPoint(300, 1.0, 0.4, 0.3);
        m_colorTF->AddRGBPoint(500, 1.0, 0.8, 0.7);

        m_opacityTF->AddPoint(-1000, 0.0);
        m_opacityTF->AddPoint(100, 0.0);
        m_opacityTF->AddPoint(150, 0.5);
        m_opacityTF->AddPoint(500, 0.9);
    }
}

InteractionState RenderServer::GetState() const
{
    return m_state;
}

void RenderServer::SetState(const InteractionState& state)
{
    std::lock_guard<std::mutex> lock(m_renderMutex);

    m_state = state;

    auto camera = m_renderer->GetActiveCamera();
    camera->SetPosition(m_state.cameraPosition);
    camera->SetFocalPoint(m_state.cameraFocalPoint);
    camera->SetViewUp(m_state.cameraViewUp);

    UpdateWindowLevel();
}

} // namespace dicom_viewer
```

### 4.2 WebSocket Server

```cpp
// websocket_server.hpp
#pragma once

#include "render_server.hpp"
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <thread>
#include <atomic>

namespace dicom_viewer {

using WsServer = websocketpp::server<websocketpp::config::asio>;
using ConnectionHdl = websocketpp::connection_hdl;
using json = nlohmann::json;

struct ClientSession {
    std::unique_ptr<RenderServer> renderServer;
    std::string studyId;
    bool isInteracting = false;
    std::chrono::steady_clock::time_point lastActivity;
};

class StreamingServer {
public:
    StreamingServer(uint16_t port);
    ~StreamingServer();

    void Start();
    void Stop();

private:
    void OnOpen(ConnectionHdl hdl);
    void OnClose(ConnectionHdl hdl);
    void OnMessage(ConnectionHdl hdl, WsServer::message_ptr msg);

    void HandleLoadStudy(ConnectionHdl hdl, const json& request);
    void HandleMouseEvent(ConnectionHdl hdl, const json& request);
    void HandleWindowLevel(ConnectionHdl hdl, const json& request);
    void HandlePreset(ConnectionHdl hdl, const json& request);
    void HandleReset(ConnectionHdl hdl);

    void SendFrame(ConnectionHdl hdl, int quality = -1);
    void SendError(ConnectionHdl hdl, const std::string& message);
    void SendJson(ConnectionHdl hdl, const json& data);

    std::string GetSessionId(ConnectionHdl hdl);

    WsServer m_server;
    uint16_t m_port;
    std::atomic<bool> m_running{false};
    std::thread m_serverThread;

    std::mutex m_sessionsMutex;
    std::unordered_map<std::string, ClientSession> m_sessions;
};

} // namespace dicom_viewer
```

```cpp
// websocket_server.cpp
#include "websocket_server.hpp"
#include "dicom_loader.hpp"  // pacs_system integration
#include <sstream>

namespace dicom_viewer {

StreamingServer::StreamingServer(uint16_t port)
    : m_port(port)
{
    m_server.init_asio();
    m_server.set_reuse_addr(true);

    m_server.set_open_handler(
        [this](ConnectionHdl hdl) { OnOpen(hdl); });
    m_server.set_close_handler(
        [this](ConnectionHdl hdl) { OnClose(hdl); });
    m_server.set_message_handler(
        [this](ConnectionHdl hdl, WsServer::message_ptr msg) {
            OnMessage(hdl, msg);
        });
}

StreamingServer::~StreamingServer()
{
    Stop();
}

void StreamingServer::Start()
{
    m_running = true;
    m_server.listen(m_port);
    m_server.start_accept();

    m_serverThread = std::thread([this]() {
        m_server.run();
    });
}

void StreamingServer::Stop()
{
    if (m_running) {
        m_running = false;
        m_server.stop_listening();
        m_server.stop();
        if (m_serverThread.joinable()) {
            m_serverThread.join();
        }
    }
}

void StreamingServer::OnOpen(ConnectionHdl hdl)
{
    std::string sessionId = GetSessionId(hdl);

    std::lock_guard<std::mutex> lock(m_sessionsMutex);

    ClientSession session;
    session.renderServer = std::make_unique<RenderServer>();
    session.renderServer->Initialize();
    session.lastActivity = std::chrono::steady_clock::now();

    m_sessions[sessionId] = std::move(session);

    // Send welcome message
    SendJson(hdl, {
        {"type", "connected"},
        {"sessionId", sessionId}
    });
}

void StreamingServer::OnClose(ConnectionHdl hdl)
{
    std::string sessionId = GetSessionId(hdl);

    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    m_sessions.erase(sessionId);
}

void StreamingServer::OnMessage(ConnectionHdl hdl, WsServer::message_ptr msg)
{
    try {
        auto request = json::parse(msg->get_payload());
        std::string action = request["action"];

        if (action == "loadStudy") {
            HandleLoadStudy(hdl, request);
        }
        else if (action == "mouse") {
            HandleMouseEvent(hdl, request);
        }
        else if (action == "windowLevel") {
            HandleWindowLevel(hdl, request);
        }
        else if (action == "preset") {
            HandlePreset(hdl, request);
        }
        else if (action == "reset") {
            HandleReset(hdl);
        }
        else if (action == "render") {
            SendFrame(hdl);
        }
    }
    catch (const std::exception& e) {
        SendError(hdl, e.what());
    }
}

void StreamingServer::HandleLoadStudy(ConnectionHdl hdl, const json& request)
{
    std::string sessionId = GetSessionId(hdl);
    std::string studyPath = request["path"];

    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    auto& session = m_sessions[sessionId];

    // Load DICOM using pacs_system + ITK
    auto imageData = DicomLoader::LoadSeries(studyPath);
    if (!imageData) {
        SendError(hdl, "Failed to load DICOM series");
        return;
    }

    session.renderServer->SetImageData(imageData);
    session.studyId = studyPath;

    SendJson(hdl, {
        {"type", "studyLoaded"},
        {"studyId", studyPath}
    });

    // Send initial frame
    SendFrame(hdl);
}

void StreamingServer::HandleMouseEvent(ConnectionHdl hdl, const json& request)
{
    std::string sessionId = GetSessionId(hdl);
    std::string eventType = request["type"];
    int x = request["x"];
    int y = request["y"];

    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    auto& session = m_sessions[sessionId];

    static int lastX = 0, lastY = 0;

    if (eventType == "mousedown") {
        session.isInteracting = true;
        lastX = x;
        lastY = y;
    }
    else if (eventType == "mousemove" && session.isInteracting) {
        int deltaX = x - lastX;
        int deltaY = y - lastY;
        lastX = x;
        lastY = y;

        // Left button drag: rotate
        if (request.contains("button") && request["button"] == 0) {
            session.renderServer->RotateCamera(deltaX * 0.5, deltaY * 0.5);
        }
        // Right button drag: zoom
        else if (request.contains("button") && request["button"] == 2) {
            double zoomFactor = 1.0 + deltaY * 0.01;
            session.renderServer->ZoomCamera(zoomFactor);
        }
        // Middle button drag: pan
        else if (request.contains("button") && request["button"] == 1) {
            session.renderServer->PanCamera(deltaX, deltaY);
        }

        // Send low-quality frame during interaction
        SendFrame(hdl, 50);
    }
    else if (eventType == "mouseup") {
        session.isInteracting = false;
        // Send high-quality frame
        SendFrame(hdl, 90);
    }
    else if (eventType == "wheel") {
        double delta = request["delta"];
        double zoomFactor = 1.0 - delta * 0.001;
        session.renderServer->ZoomCamera(zoomFactor);
        SendFrame(hdl);
    }
}

void StreamingServer::HandleWindowLevel(ConnectionHdl hdl, const json& request)
{
    std::string sessionId = GetSessionId(hdl);
    double window = request["window"];
    double level = request["level"];

    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    auto& session = m_sessions[sessionId];

    session.renderServer->SetWindowLevel(window, level);
    SendFrame(hdl);
}

void StreamingServer::HandlePreset(ConnectionHdl hdl, const json& request)
{
    std::string sessionId = GetSessionId(hdl);
    std::string presetName = request["preset"];

    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    auto& session = m_sessions[sessionId];

    session.renderServer->ApplyPreset(presetName);
    SendFrame(hdl);
}

void StreamingServer::HandleReset(ConnectionHdl hdl)
{
    std::string sessionId = GetSessionId(hdl);

    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    auto& session = m_sessions[sessionId];

    session.renderServer->ResetCamera();
    SendFrame(hdl);
}

void StreamingServer::SendFrame(ConnectionHdl hdl, int quality)
{
    std::string sessionId = GetSessionId(hdl);

    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    auto& session = m_sessions[sessionId];

    auto jpegData = session.renderServer->RenderToJPEG(quality);

    m_server.send(hdl, jpegData.data(), jpegData.size(),
                  websocketpp::frame::opcode::binary);
}

void StreamingServer::SendError(ConnectionHdl hdl, const std::string& message)
{
    SendJson(hdl, {
        {"type", "error"},
        {"message", message}
    });
}

void StreamingServer::SendJson(ConnectionHdl hdl, const json& data)
{
    m_server.send(hdl, data.dump(), websocketpp::frame::opcode::text);
}

std::string StreamingServer::GetSessionId(ConnectionHdl hdl)
{
    auto conn = m_server.get_con_from_hdl(hdl);
    std::stringstream ss;
    ss << conn.get();
    return ss.str();
}

} // namespace dicom_viewer
```

---

## 5. Client Implementation

### 5.1 Web Client (TypeScript/React)

```typescript
// RemoteDicomViewer.tsx
import React, { useEffect, useRef, useState, useCallback } from 'react';

interface ViewerProps {
  serverUrl: string;
  studyPath?: string;
}

interface ViewerState {
  connected: boolean;
  loading: boolean;
  windowWidth: number;
  windowLevel: number;
  preset: string;
}

export const RemoteDicomViewer: React.FC<ViewerProps> = ({
  serverUrl,
  studyPath
}) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const wsRef = useRef<WebSocket | null>(null);
  const [state, setState] = useState<ViewerState>({
    connected: false,
    loading: false,
    windowWidth: 400,
    windowLevel: 40,
    preset: 'CT_Bone'
  });

  // WebSocket connection
  useEffect(() => {
    const ws = new WebSocket(serverUrl);
    ws.binaryType = 'arraybuffer';

    ws.onopen = () => {
      setState(s => ({ ...s, connected: true }));
      if (studyPath) {
        ws.send(JSON.stringify({
          action: 'loadStudy',
          path: studyPath
        }));
      }
    };

    ws.onmessage = (event) => {
      if (event.data instanceof ArrayBuffer) {
        // Binary frame data
        displayFrame(event.data);
      } else {
        // JSON message
        const msg = JSON.parse(event.data);
        handleServerMessage(msg);
      }
    };

    ws.onclose = () => {
      setState(s => ({ ...s, connected: false }));
    };

    wsRef.current = ws;

    return () => {
      ws.close();
    };
  }, [serverUrl, studyPath]);

  const displayFrame = useCallback((data: ArrayBuffer) => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const blob = new Blob([data], { type: 'image/jpeg' });
    const url = URL.createObjectURL(blob);

    const img = new Image();
    img.onload = () => {
      canvas.width = img.width;
      canvas.height = img.height;
      ctx.drawImage(img, 0, 0);
      URL.revokeObjectURL(url);
    };
    img.src = url;
  }, []);

  const handleServerMessage = useCallback((msg: any) => {
    switch (msg.type) {
      case 'studyLoaded':
        setState(s => ({ ...s, loading: false }));
        break;
      case 'error':
        console.error('Server error:', msg.message);
        break;
    }
  }, []);

  // Mouse interaction handlers
  const handleMouseDown = useCallback((e: React.MouseEvent) => {
    wsRef.current?.send(JSON.stringify({
      action: 'mouse',
      type: 'mousedown',
      x: e.clientX,
      y: e.clientY,
      button: e.button
    }));
  }, []);

  const handleMouseMove = useCallback((e: React.MouseEvent) => {
    if (e.buttons > 0) {
      wsRef.current?.send(JSON.stringify({
        action: 'mouse',
        type: 'mousemove',
        x: e.clientX,
        y: e.clientY,
        button: e.buttons === 1 ? 0 : e.buttons === 2 ? 2 : 1
      }));
    }
  }, []);

  const handleMouseUp = useCallback((e: React.MouseEvent) => {
    wsRef.current?.send(JSON.stringify({
      action: 'mouse',
      type: 'mouseup',
      x: e.clientX,
      y: e.clientY,
      button: e.button
    }));
  }, []);

  const handleWheel = useCallback((e: React.WheelEvent) => {
    wsRef.current?.send(JSON.stringify({
      action: 'mouse',
      type: 'wheel',
      delta: e.deltaY
    }));
  }, []);

  // Control handlers
  const handleWindowLevelChange = useCallback((window: number, level: number) => {
    setState(s => ({ ...s, windowWidth: window, windowLevel: level }));
    wsRef.current?.send(JSON.stringify({
      action: 'windowLevel',
      window,
      level
    }));
  }, []);

  const handlePresetChange = useCallback((preset: string) => {
    setState(s => ({ ...s, preset }));
    wsRef.current?.send(JSON.stringify({
      action: 'preset',
      preset
    }));
  }, []);

  const handleReset = useCallback(() => {
    wsRef.current?.send(JSON.stringify({ action: 'reset' }));
  }, []);

  return (
    <div className="remote-dicom-viewer">
      <div className="viewport-container">
        <canvas
          ref={canvasRef}
          onMouseDown={handleMouseDown}
          onMouseMove={handleMouseMove}
          onMouseUp={handleMouseUp}
          onWheel={handleWheel}
          onContextMenu={(e) => e.preventDefault()}
        />
        {!state.connected && (
          <div className="overlay">Connecting...</div>
        )}
        {state.loading && (
          <div className="overlay">Loading study...</div>
        )}
      </div>

      <div className="controls">
        <div className="control-group">
          <label>Window Width</label>
          <input
            type="range"
            min="1"
            max="4000"
            value={state.windowWidth}
            onChange={(e) => handleWindowLevelChange(
              parseInt(e.target.value),
              state.windowLevel
            )}
          />
          <span>{state.windowWidth}</span>
        </div>

        <div className="control-group">
          <label>Window Level</label>
          <input
            type="range"
            min="-1000"
            max="3000"
            value={state.windowLevel}
            onChange={(e) => handleWindowLevelChange(
              state.windowWidth,
              parseInt(e.target.value)
            )}
          />
          <span>{state.windowLevel}</span>
        </div>

        <div className="control-group">
          <label>Preset</label>
          <select
            value={state.preset}
            onChange={(e) => handlePresetChange(e.target.value)}
          >
            <option value="CT_Bone">CT Bone</option>
            <option value="CT_SoftTissue">CT Soft Tissue</option>
            <option value="CT_Lung">CT Lung</option>
            <option value="CT_Angio">CT Angio</option>
          </select>
        </div>

        <button onClick={handleReset}>Reset View</button>
      </div>
    </div>
  );
};
```

### 5.2 Mobile Client (Flutter)

```dart
// remote_dicom_viewer.dart
import 'dart:typed_data';
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:web_socket_channel/web_socket_channel.dart';

class RemoteDicomViewer extends StatefulWidget {
  final String serverUrl;
  final String? studyPath;

  const RemoteDicomViewer({
    Key? key,
    required this.serverUrl,
    this.studyPath,
  }) : super(key: key);

  @override
  _RemoteDicomViewerState createState() => _RemoteDicomViewerState();
}

class _RemoteDicomViewerState extends State<RemoteDicomViewer> {
  WebSocketChannel? _channel;
  Uint8List? _currentFrame;
  bool _connected = false;
  double _windowWidth = 400;
  double _windowLevel = 40;
  String _preset = 'CT_Bone';

  Offset? _lastPanPosition;

  @override
  void initState() {
    super.initState();
    _connect();
  }

  void _connect() {
    _channel = WebSocketChannel.connect(Uri.parse(widget.serverUrl));

    _channel!.stream.listen(
      (data) {
        if (data is Uint8List) {
          setState(() {
            _currentFrame = data;
          });
        } else if (data is String) {
          _handleMessage(jsonDecode(data));
        }
      },
      onDone: () {
        setState(() {
          _connected = false;
        });
      },
    );

    setState(() {
      _connected = true;
    });

    if (widget.studyPath != null) {
      _sendMessage({
        'action': 'loadStudy',
        'path': widget.studyPath,
      });
    }
  }

  void _handleMessage(Map<String, dynamic> msg) {
    switch (msg['type']) {
      case 'connected':
        print('Connected: ${msg['sessionId']}');
        break;
      case 'error':
        print('Error: ${msg['message']}');
        break;
    }
  }

  void _sendMessage(Map<String, dynamic> msg) {
    _channel?.sink.add(jsonEncode(msg));
  }

  void _onPanStart(DragStartDetails details) {
    _lastPanPosition = details.localPosition;
    _sendMessage({
      'action': 'mouse',
      'type': 'mousedown',
      'x': details.localPosition.dx.toInt(),
      'y': details.localPosition.dy.toInt(),
      'button': 0,
    });
  }

  void _onPanUpdate(DragUpdateDetails details) {
    _sendMessage({
      'action': 'mouse',
      'type': 'mousemove',
      'x': details.localPosition.dx.toInt(),
      'y': details.localPosition.dy.toInt(),
      'button': 0,
    });
  }

  void _onPanEnd(DragEndDetails details) {
    _sendMessage({
      'action': 'mouse',
      'type': 'mouseup',
      'x': _lastPanPosition?.dx.toInt() ?? 0,
      'y': _lastPanPosition?.dy.toInt() ?? 0,
      'button': 0,
    });
  }

  void _onScaleUpdate(ScaleUpdateDetails details) {
    if (details.scale != 1.0) {
      final delta = (details.scale - 1.0) * 100;
      _sendMessage({
        'action': 'mouse',
        'type': 'wheel',
        'delta': -delta,
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('DICOM Viewer'),
        actions: [
          IconButton(
            icon: const Icon(Icons.refresh),
            onPressed: () => _sendMessage({'action': 'reset'}),
          ),
        ],
      ),
      body: Column(
        children: [
          Expanded(
            child: GestureDetector(
              onPanStart: _onPanStart,
              onPanUpdate: _onPanUpdate,
              onPanEnd: _onPanEnd,
              onScaleUpdate: _onScaleUpdate,
              child: Container(
                color: Colors.black,
                child: _currentFrame != null
                    ? Image.memory(
                        _currentFrame!,
                        fit: BoxFit.contain,
                        gaplessPlayback: true,
                      )
                    : const Center(
                        child: CircularProgressIndicator(),
                      ),
              ),
            ),
          ),
          _buildControls(),
        ],
      ),
    );
  }

  Widget _buildControls() {
    return Container(
      padding: const EdgeInsets.all(16),
      child: Column(
        children: [
          Row(
            children: [
              const Text('Window: '),
              Expanded(
                child: Slider(
                  value: _windowWidth,
                  min: 1,
                  max: 4000,
                  onChanged: (value) {
                    setState(() {
                      _windowWidth = value;
                    });
                    _sendMessage({
                      'action': 'windowLevel',
                      'window': value,
                      'level': _windowLevel,
                    });
                  },
                ),
              ),
              Text(_windowWidth.toInt().toString()),
            ],
          ),
          Row(
            children: [
              const Text('Level: '),
              Expanded(
                child: Slider(
                  value: _windowLevel,
                  min: -1000,
                  max: 3000,
                  onChanged: (value) {
                    setState(() {
                      _windowLevel = value;
                    });
                    _sendMessage({
                      'action': 'windowLevel',
                      'window': _windowWidth,
                      'level': value,
                    });
                  },
                ),
              ),
              Text(_windowLevel.toInt().toString()),
            ],
          ),
          DropdownButton<String>(
            value: _preset,
            items: const [
              DropdownMenuItem(value: 'CT_Bone', child: Text('CT Bone')),
              DropdownMenuItem(value: 'CT_SoftTissue', child: Text('Soft Tissue')),
              DropdownMenuItem(value: 'CT_Lung', child: Text('Lung')),
              DropdownMenuItem(value: 'CT_Angio', child: Text('Angio')),
            ],
            onChanged: (value) {
              if (value != null) {
                setState(() {
                  _preset = value;
                });
                _sendMessage({
                  'action': 'preset',
                  'preset': value,
                });
              }
            },
          ),
        ],
      ),
    );
  }

  @override
  void dispose() {
    _channel?.sink.close();
    super.dispose();
  }
}
```

---

## 6. Optimization Strategies

### 6.1 Adaptive Quality

```cpp
// adaptive_quality.hpp
#pragma once

#include <chrono>

namespace dicom_viewer {

class AdaptiveQualityManager {
public:
    struct QualitySettings {
        int jpegQuality;        // 1-100
        float resolutionScale;  // 0.25-1.0
        int targetFps;
    };

    QualitySettings GetSettings(bool isInteracting, int networkLatencyMs) {
        if (isInteracting) {
            // During interaction: prioritize responsiveness
            if (networkLatencyMs > 100) {
                return { 30, 0.25f, 30 };  // Very low quality
            } else if (networkLatencyMs > 50) {
                return { 50, 0.5f, 30 };   // Low quality
            } else {
                return { 60, 0.75f, 30 };  // Medium quality
            }
        } else {
            // Idle: prioritize quality
            return { 95, 1.0f, 1 };  // High quality
        }
    }

    // Progressive refinement schedule
    std::vector<QualitySettings> GetProgressiveSchedule() {
        return {
            { 50, 0.25f, 0 },   // Immediate: very low
            { 70, 0.5f, 100 },  // After 100ms: medium
            { 90, 1.0f, 500 }   // After 500ms: high
        };
    }
};

} // namespace dicom_viewer
```

### 6.2 Delta Encoding

```cpp
// delta_encoder.hpp
#pragma once

#include <vtkImageData.h>
#include <vector>
#include <cstdint>

namespace dicom_viewer {

struct DirtyRegion {
    int x, y, width, height;
};

class DeltaEncoder {
public:
    DeltaEncoder(int tileSize = 64);

    // Find regions that changed between frames
    std::vector<DirtyRegion> FindDirtyRegions(
        const uint8_t* currentFrame,
        const uint8_t* previousFrame,
        int width, int height, int channels);

    // Encode only dirty regions
    std::vector<uint8_t> EncodeDirtyRegions(
        const uint8_t* frame,
        int width, int height, int channels,
        const std::vector<DirtyRegion>& regions);

private:
    int m_tileSize;
    std::vector<uint32_t> m_previousTileHashes;

    uint32_t HashTile(const uint8_t* data, int stride,
                      int x, int y, int width, int height, int channels);
};

} // namespace dicom_viewer
```

---

## 7. Deployment Architecture

### 7.1 Docker Deployment

```yaml
# docker-compose.yml
version: '3.8'

services:
  render-server:
    build:
      context: .
      dockerfile: Dockerfile.render
    deploy:
      resources:
        reservations:
          devices:
            - driver: nvidia
              count: 1
              capabilities: [gpu]
    ports:
      - "8080:8080"
    volumes:
      - dicom-data:/data
    environment:
      - NVIDIA_VISIBLE_DEVICES=all
      - RENDER_WIDTH=1920
      - RENDER_HEIGHT=1080

  load-balancer:
    image: nginx:alpine
    ports:
      - "80:80"
      - "443:443"
    volumes:
      - ./nginx.conf:/etc/nginx/nginx.conf:ro
      - ./ssl:/etc/nginx/ssl:ro
    depends_on:
      - render-server

  session-store:
    image: redis:alpine
    volumes:
      - redis-data:/data

volumes:
  dicom-data:
  redis-data:
```

```dockerfile
# Dockerfile.render
FROM nvidia/cuda:12.0-runtime-ubuntu22.04

# Install VTK and dependencies
RUN apt-get update && apt-get install -y \
    libvtk9-dev \
    libgl1-mesa-glx \
    libosmesa6-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy application
COPY build/render_server /app/render_server
COPY presets/ /app/presets/

WORKDIR /app
EXPOSE 8080

CMD ["./render_server", "--port", "8080"]
```

### 7.2 Kubernetes Deployment

```yaml
# k8s-deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: dicom-render-server
spec:
  replicas: 3
  selector:
    matchLabels:
      app: dicom-render
  template:
    metadata:
      labels:
        app: dicom-render
    spec:
      containers:
      - name: render-server
        image: dicom-viewer/render-server:latest
        ports:
        - containerPort: 8080
        resources:
          limits:
            nvidia.com/gpu: 1
            memory: "8Gi"
          requests:
            memory: "4Gi"
        env:
        - name: RENDER_WIDTH
          value: "1920"
        - name: RENDER_HEIGHT
          value: "1080"
---
apiVersion: v1
kind: Service
metadata:
  name: dicom-render-service
spec:
  selector:
    app: dicom-render
  ports:
  - port: 80
    targetPort: 8080
  type: LoadBalancer
```

---

## 8. Performance Benchmarks

### 8.1 Expected Performance

| Metric | Target | Condition |
|--------|--------|-----------|
| **Frame Latency** | ≤ 100ms | 1080p JPEG, LAN |
| **Interaction FPS** | ≥ 20 FPS | Low quality mode |
| **Final Quality FPS** | ≥ 5 FPS | High quality mode |
| **Concurrent Users** | 10+ per GPU | NVIDIA RTX 3080 |
| **Network Bandwidth** | ~2 MB/s | Per active session |

### 8.2 Optimization Checklist

- [ ] GPU-accelerated encoding (NVENC)
- [ ] Adaptive quality based on network conditions
- [ ] Delta encoding for static regions
- [ ] Progressive refinement on idle
- [ ] Connection pooling for WebSocket
- [ ] Redis session caching
- [ ] CDN for static assets

---

## 9. References

### Existing Solutions
- [ParaView Web](https://kitware.github.io/paraviewweb/) - VTK-based remote visualization
- [3D Slicer Web](https://www.slicer.org/) - Medical imaging platform with web support
- [OHIF Viewer](https://ohif.org/) - Open-source web DICOM viewer

### Technologies
- [VTK Offscreen Rendering](https://vtk.org/doc/nightly/html/classvtkRenderWindow.html)
- [WebSocket++](https://github.com/zaphoyd/websocketpp)
- [NVENC Video Encoding](https://developer.nvidia.com/nvidia-video-codec-sdk)

---

*Previous Document: [06-gui-framework-comparison.md](06-gui-framework-comparison.md) - GUI Framework Comparison*
*Back to Index: [README.md](README.md) - Document Index*

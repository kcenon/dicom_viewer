// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "ui/display_3d_controller.hpp"

#include "services/hemodynamic_surface_manager.hpp"
#include "services/render/asc_view_controller.hpp"
#include "services/surface_renderer.hpp"
#include "services/volume_renderer.hpp"

#include <vtkActor.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>

#include <array>
#include <map>

namespace dicom_viewer::ui {

// Overlay name constants matching the names used in VolumeRenderer::addScalarOverlay
static constexpr const char* kVelocityOverlay = "velocity";
static constexpr const char* kVorticityOverlay = "vorticity";
static constexpr const char* kEnergyLossOverlay = "energy_loss";
static constexpr const char* kMagnitudeOverlay = "magnitude";

class Display3DController::Impl {
public:
    services::VolumeRenderer* volumeRenderer = nullptr;
    services::SurfaceRenderer* surfaceRenderer = nullptr;
    services::HemodynamicSurfaceManager* hemoManager = nullptr;
    services::AscViewController* ascController = nullptr;

    vtkSmartPointer<vtkActor> streamlineActor;
    vtkSmartPointer<vtkActor> maskVolumeActor;
    vtkSmartPointer<vtkActor> surfaceActor;

    std::array<bool, 13> enabled{};  // All false by default

    // Scalar range per item (only for colormap items)
    std::map<Display3DItem, std::pair<double, double>> scalarRanges;
};

Display3DController::Display3DController()
    : impl_(std::make_unique<Impl>())
{}

Display3DController::~Display3DController() = default;
Display3DController::Display3DController(Display3DController&&) noexcept = default;
Display3DController& Display3DController::operator=(Display3DController&&) noexcept = default;

void Display3DController::setVolumeRenderer(services::VolumeRenderer* renderer)
{
    impl_->volumeRenderer = renderer;
}

void Display3DController::setSurfaceRenderer(services::SurfaceRenderer* renderer)
{
    impl_->surfaceRenderer = renderer;
}

void Display3DController::setHemodynamicManager(services::HemodynamicSurfaceManager* manager)
{
    impl_->hemoManager = manager;
}

void Display3DController::setStreamlineActor(vtkSmartPointer<vtkActor> actor)
{
    impl_->streamlineActor = std::move(actor);
}

void Display3DController::setMaskVolumeActor(vtkSmartPointer<vtkActor> actor)
{
    impl_->maskVolumeActor = std::move(actor);
}

void Display3DController::setSurfaceActor(vtkSmartPointer<vtkActor> actor)
{
    impl_->surfaceActor = std::move(actor);
}

void Display3DController::setAscController(services::AscViewController* controller)
{
    impl_->ascController = controller;
}

void Display3DController::handleToggle(Display3DItem item, bool enabled)
{
    auto idx = static_cast<int>(item);
    if (idx >= 0 && idx < 13) {
        impl_->enabled[idx] = enabled;
    }

    switch (item) {
    case Display3DItem::MaskVolume:
        if (impl_->maskVolumeActor) {
            impl_->maskVolumeActor->SetVisibility(enabled ? 1 : 0);
        }
        break;

    case Display3DItem::Surface:
        if (impl_->surfaceActor) {
            impl_->surfaceActor->SetVisibility(enabled ? 1 : 0);
        }
        break;

    case Display3DItem::Cine:
        // Stub: cine playback not yet implemented in 3D
        break;

    case Display3DItem::Magnitude:
        if (impl_->volumeRenderer) {
            impl_->volumeRenderer->setOverlayVisible(kMagnitudeOverlay, enabled);
        }
        break;

    case Display3DItem::Velocity:
        if (impl_->volumeRenderer) {
            impl_->volumeRenderer->setOverlayVisible(kVelocityOverlay, enabled);
        }
        break;

    case Display3DItem::ASC:
        if (impl_->ascController) {
            impl_->ascController->setVisible(enabled);
        }
        break;

    case Display3DItem::Streamline:
        if (impl_->streamlineActor) {
            impl_->streamlineActor->SetVisibility(enabled ? 1 : 0);
        }
        break;

    case Display3DItem::EnergyLoss:
        if (impl_->volumeRenderer) {
            impl_->volumeRenderer->setOverlayVisible(kEnergyLossOverlay, enabled);
        }
        break;

    case Display3DItem::WSS:
        if (impl_->surfaceRenderer && impl_->hemoManager) {
            if (auto wss = impl_->hemoManager->wssIndex()) {
                impl_->surfaceRenderer->setSurfaceVisibility(*wss, enabled);
            }
        }
        break;

    case Display3DItem::OSI:
        if (impl_->surfaceRenderer && impl_->hemoManager) {
            if (auto osi = impl_->hemoManager->osiIndex()) {
                impl_->surfaceRenderer->setSurfaceVisibility(*osi, enabled);
            }
        }
        break;

    case Display3DItem::AFI:
        if (impl_->surfaceRenderer && impl_->hemoManager) {
            if (auto afi = impl_->hemoManager->afiIndex()) {
                impl_->surfaceRenderer->setSurfaceVisibility(*afi, enabled);
            }
        }
        break;

    case Display3DItem::RRT:
        if (impl_->surfaceRenderer && impl_->hemoManager) {
            if (auto rrt = impl_->hemoManager->rrtIndex()) {
                impl_->surfaceRenderer->setSurfaceVisibility(*rrt, enabled);
            }
        }
        break;

    case Display3DItem::Vorticity:
        if (impl_->volumeRenderer) {
            impl_->volumeRenderer->setOverlayVisible(kVorticityOverlay, enabled);
        }
        break;
    }
}

void Display3DController::setScalarRange(Display3DItem item, double minVal, double maxVal)
{
    if (!hasColormapRange(item)) return;

    impl_->scalarRanges[item] = {minVal, maxVal};

    switch (item) {
    // Surface hemodynamic parameters
    case Display3DItem::WSS:
        if (impl_->surfaceRenderer && impl_->hemoManager) {
            if (auto idx = impl_->hemoManager->wssIndex()) {
                impl_->surfaceRenderer->setSurfaceScalarRange(*idx, minVal, maxVal);
                impl_->surfaceRenderer->setSurfaceLookupTable(
                    *idx, services::SurfaceRenderer::createWSSLookupTable(maxVal));
            }
        }
        break;

    case Display3DItem::OSI:
        if (impl_->surfaceRenderer && impl_->hemoManager) {
            if (auto idx = impl_->hemoManager->osiIndex()) {
                impl_->surfaceRenderer->setSurfaceScalarRange(*idx, minVal, maxVal);
                impl_->surfaceRenderer->setSurfaceLookupTable(
                    *idx, services::SurfaceRenderer::createOSILookupTable());
            }
        }
        break;

    case Display3DItem::AFI:
        if (impl_->surfaceRenderer && impl_->hemoManager) {
            if (auto idx = impl_->hemoManager->afiIndex()) {
                impl_->surfaceRenderer->setSurfaceScalarRange(*idx, minVal, maxVal);
                impl_->surfaceRenderer->setSurfaceLookupTable(
                    *idx, services::SurfaceRenderer::createAFILookupTable(maxVal));
            }
        }
        break;

    case Display3DItem::RRT:
        if (impl_->surfaceRenderer && impl_->hemoManager) {
            if (auto idx = impl_->hemoManager->rrtIndex()) {
                impl_->surfaceRenderer->setSurfaceScalarRange(*idx, minVal, maxVal);
                impl_->surfaceRenderer->setSurfaceLookupTable(
                    *idx, services::SurfaceRenderer::createRRTLookupTable(maxVal));
            }
        }
        break;

    // Volume overlay parameters
    case Display3DItem::Velocity:
        if (impl_->volumeRenderer && impl_->volumeRenderer->hasOverlay(kVelocityOverlay)) {
            impl_->volumeRenderer->updateOverlayTransferFunctions(
                kVelocityOverlay,
                services::VolumeRenderer::createVelocityColorFunction(maxVal),
                services::VolumeRenderer::createVelocityOpacityFunction(maxVal));
        }
        break;

    case Display3DItem::Vorticity:
        if (impl_->volumeRenderer && impl_->volumeRenderer->hasOverlay(kVorticityOverlay)) {
            impl_->volumeRenderer->updateOverlayTransferFunctions(
                kVorticityOverlay,
                services::VolumeRenderer::createVorticityColorFunction(maxVal),
                services::VolumeRenderer::createVorticityOpacityFunction(maxVal));
        }
        break;

    case Display3DItem::EnergyLoss:
        if (impl_->volumeRenderer && impl_->volumeRenderer->hasOverlay(kEnergyLossOverlay)) {
            impl_->volumeRenderer->updateOverlayTransferFunctions(
                kEnergyLossOverlay,
                services::VolumeRenderer::createEnergyLossColorFunction(maxVal),
                services::VolumeRenderer::createEnergyLossOpacityFunction(maxVal));
        }
        break;

    case Display3DItem::Magnitude:
        if (impl_->volumeRenderer && impl_->volumeRenderer->hasOverlay(kMagnitudeOverlay)) {
            impl_->volumeRenderer->updateOverlayTransferFunctions(
                kMagnitudeOverlay,
                services::VolumeRenderer::createVelocityColorFunction(maxVal),
                services::VolumeRenderer::createVelocityOpacityFunction(maxVal));
        }
        break;

    default:
        break;
    }
}

std::pair<double, double> Display3DController::scalarRange(Display3DItem item) const
{
    auto it = impl_->scalarRanges.find(item);
    if (it != impl_->scalarRanges.end()) {
        return it->second;
    }
    return {0.0, 0.0};
}

bool Display3DController::hasColormapRange(Display3DItem item)
{
    switch (item) {
    case Display3DItem::WSS:
    case Display3DItem::OSI:
    case Display3DItem::AFI:
    case Display3DItem::RRT:
    case Display3DItem::Velocity:
    case Display3DItem::Vorticity:
    case Display3DItem::EnergyLoss:
    case Display3DItem::Magnitude:
        return true;
    default:
        return false;
    }
}

bool Display3DController::isEnabled(Display3DItem item) const
{
    auto idx = static_cast<int>(item);
    if (idx >= 0 && idx < 13) {
        return impl_->enabled[idx];
    }
    return false;
}

std::array<bool, 13> Display3DController::enabledStates() const
{
    return impl_->enabled;
}

}  // namespace dicom_viewer::ui

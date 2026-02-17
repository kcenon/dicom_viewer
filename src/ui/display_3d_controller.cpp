#include "ui/display_3d_controller.hpp"

#include "services/hemodynamic_surface_manager.hpp"
#include "services/surface_renderer.hpp"
#include "services/volume_renderer.hpp"

#include <vtkActor.h>

#include <array>

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

    vtkSmartPointer<vtkActor> streamlineActor;
    vtkSmartPointer<vtkActor> maskVolumeActor;
    vtkSmartPointer<vtkActor> surfaceActor;

    std::array<bool, 13> enabled{};  // All false by default
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
        // Stub: ASC view not yet implemented
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

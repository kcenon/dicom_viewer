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


/**
 * @file display_3d_controller.hpp
 * @brief Routes Display 3D checkbox toggles to rendering backends
 * @details Maps Display3DItem toggles (Volume, Surface, Streamline, etc.)
 *          to renderer visibility calls. Does not derive from QObject;
 *          caller wires FlowToolPanel::display3DToggled to handleToggle()
 *          via lambda.
 *
 * @author kcenon
 * @since 1.0.0
 */
#pragma once

#include <memory>

#include <vtkSmartPointer.h>

#include "ui/panels/flow_tool_panel.hpp"

class vtkActor;
class vtkRenderer;

namespace dicom_viewer::services {
class VolumeRenderer;
class SurfaceRenderer;
class HemodynamicSurfaceManager;
class AscViewController;
}  // namespace dicom_viewer::services

namespace dicom_viewer::ui {

/**
 * @brief Routes Display 3D checkbox toggles to rendering backends
 *
 * Maps each Display3DItem to the appropriate renderer visibility call:
 * - Volume overlays (Velocity, Vorticity, EnergyLoss, Magnitude)
 *   → VolumeRenderer::setOverlayVisible()
 * - Surface parameters (WSS, OSI, AFI, RRT)
 *   → SurfaceRenderer::setSurfaceVisibility() via HemodynamicSurfaceManager
 * - Streamline → vtkActor visibility
 * - MaskVolume, Surface → base renderer visibility
 * - ASC → AscViewController::setVisible()
 * - Cine → stub (not yet implemented)
 *
 * This class does NOT derive from QObject. The caller (MainWindow) wires
 * FlowToolPanel::display3DToggled to handleToggle() via a lambda.
 *
 * @trace SRS-FR-047, PRD FR-016
 */
class Display3DController {
public:
    Display3DController();
    ~Display3DController();

    // Non-copyable, movable
    Display3DController(const Display3DController&) = delete;
    Display3DController& operator=(const Display3DController&) = delete;
    Display3DController(Display3DController&&) noexcept;
    Display3DController& operator=(Display3DController&&) noexcept;

    // --- Renderer bindings ---

    /**
     * @brief Set the volume renderer for overlay visibility control
     * @param renderer Non-owning pointer (caller manages lifetime)
     */
    void setVolumeRenderer(services::VolumeRenderer* renderer);

    /**
     * @brief Set the surface renderer for surface visibility control
     * @param renderer Non-owning pointer (caller manages lifetime)
     */
    void setSurfaceRenderer(services::SurfaceRenderer* renderer);

    /**
     * @brief Set the hemodynamic surface manager for WSS/OSI/AFI/RRT index lookups
     * @param manager Non-owning pointer (caller manages lifetime)
     */
    void setHemodynamicManager(services::HemodynamicSurfaceManager* manager);

    /**
     * @brief Set the streamline actor for visibility toggling
     * @param actor Streamline geometry actor
     */
    void setStreamlineActor(vtkSmartPointer<vtkActor> actor);

    /**
     * @brief Set the mask volume actor for visibility toggling
     * @param actor Mask volume rendering actor (vtkActor or vtkVolume cast)
     */
    void setMaskVolumeActor(vtkSmartPointer<vtkActor> actor);

    /**
     * @brief Set the isosurface actor for visibility toggling
     * @param actor Surface mesh actor
     */
    void setSurfaceActor(vtkSmartPointer<vtkActor> actor);

    /**
     * @brief Set the ASC view controller for orthogonal plane visibility
     * @param controller Non-owning pointer (caller manages lifetime)
     */
    void setAscController(services::AscViewController* controller);

    // --- Toggle dispatch ---

    /**
     * @brief Handle a Display 3D checkbox toggle
     *
     * Routes the toggle to the appropriate renderer based on item type.
     * Silently ignores items whose renderer has not been set.
     *
     * @param item The Display3DItem toggled
     * @param enabled True to show, false to hide
     */
    void handleToggle(Display3DItem item, bool enabled);

    // --- Scalar range control ---

    /**
     * @brief Set the colormap scalar range for a Display 3D item
     *
     * Routes range changes to the appropriate renderer:
     * - WSS/OSI/AFI/RRT → SurfaceRenderer::setSurfaceScalarRange + LUT rebuild
     * - Velocity/Vorticity/EnergyLoss/Magnitude → VolumeRenderer TF rebuild
     * - Other items are no-op
     *
     * @param item The Display3DItem to configure
     * @param minVal Minimum scalar value (mapped to colormap start)
     * @param maxVal Maximum scalar value (mapped to colormap end)
     */
    void setScalarRange(Display3DItem item, double minVal, double maxVal);

    /**
     * @brief Get the current scalar range for a Display 3D item
     * @return {min, max} pair, or {0, 0} if item has no range
     */
    [[nodiscard]] std::pair<double, double> scalarRange(Display3DItem item) const;

    /**
     * @brief Check if a Display 3D item supports colormap range adjustment
     */
    [[nodiscard]] static bool hasColormapRange(Display3DItem item);

    // --- State queries ---

    /**
     * @brief Check if a Display 3D item is currently enabled
     */
    [[nodiscard]] bool isEnabled(Display3DItem item) const;

    /**
     * @brief Get the enabled state for all 13 items (indexed by enum ordinal)
     */
    [[nodiscard]] std::array<bool, 13> enabledStates() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::ui

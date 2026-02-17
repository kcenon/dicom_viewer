#pragma once

#include <memory>
#include <optional>

#include <vtkSmartPointer.h>

class vtkPolyData;

namespace dicom_viewer::services {

class SurfaceRenderer;

/**
 * @brief Coordinates hemodynamic parameter visualization on vessel wall meshes
 *
 * Wires VesselAnalyzer analysis results to SurfaceRenderer's per-vertex
 * scalar coloring API. Each hemodynamic parameter (WSS, OSI, AFI, RRT)
 * is independently toggleable and has its own colormap.
 *
 * This class does NOT depend on VesselAnalyzer or flow_service — it operates
 * on generic vtkPolyData with named scalar arrays. The caller decomposes
 * VesselAnalyzer results before passing them here.
 *
 * Typical usage:
 * @code
 * HemodynamicSurfaceManager manager;
 * auto wssIdx = manager.showWSS(renderer, wssResult.wallMesh, wssResult.maxWSS);
 * auto osiIdx = manager.showOSI(renderer, osiResult.wallMesh);
 * auto afiIdx = manager.showAFI(renderer, tawssSurface);
 * auto rrtIdx = manager.showRRT(renderer, rrtSurface, maxRRT);
 * @endcode
 *
 * @trace SRS-FR-047, PRD FR-016
 */
class HemodynamicSurfaceManager {
public:
    HemodynamicSurfaceManager();
    ~HemodynamicSurfaceManager();

    // Non-copyable, movable
    HemodynamicSurfaceManager(const HemodynamicSurfaceManager&) = delete;
    HemodynamicSurfaceManager& operator=(const HemodynamicSurfaceManager&) = delete;
    HemodynamicSurfaceManager(HemodynamicSurfaceManager&&) noexcept;
    HemodynamicSurfaceManager& operator=(HemodynamicSurfaceManager&&) noexcept;

    /**
     * @brief Display WSS coloring on the vessel wall
     *
     * Uses blue-to-red sequential colormap via SurfaceRenderer::createWSSLookupTable.
     * Scalar range is set to [0, maxWSS].
     *
     * @param renderer Target surface renderer
     * @param wallMesh Mesh with per-vertex "WSS" array
     * @param maxWSS Maximum WSS value in Pa for colormap scaling
     * @return Surface index in the renderer
     */
    size_t showWSS(SurfaceRenderer& renderer,
                   vtkSmartPointer<vtkPolyData> wallMesh,
                   double maxWSS);

    /**
     * @brief Display OSI coloring on the vessel wall
     *
     * Uses blue-white-red diverging colormap via SurfaceRenderer::createOSILookupTable.
     * Scalar range is fixed to [0, 0.5] (OSI definition range).
     *
     * @param renderer Target surface renderer
     * @param wallMesh Mesh with per-vertex "OSI" array
     * @return Surface index in the renderer
     */
    size_t showOSI(SurfaceRenderer& renderer,
                   vtkSmartPointer<vtkPolyData> wallMesh);

    /**
     * @brief Display AFI coloring on the vessel wall
     *
     * AFI = TAWSS_local / mean(TAWSS) at each vertex.
     * Input surface must have a "TAWSS" point data array.
     * Computes AFI from the TAWSS array and applies green-yellow-red colormap.
     *
     * @param renderer Target surface renderer
     * @param tawssSurface Mesh with per-vertex "TAWSS" array
     * @return Surface index in the renderer
     */
    size_t showAFI(SurfaceRenderer& renderer,
                   vtkSmartPointer<vtkPolyData> tawssSurface);

    /**
     * @brief Display RRT coloring on the vessel wall
     *
     * Uses yellow-to-red sequential colormap via SurfaceRenderer::createRRTLookupTable.
     *
     * @param renderer Target surface renderer
     * @param rrtSurface Mesh with per-vertex "RRT" array
     * @param maxRRT Maximum RRT value for colormap scaling
     * @return Surface index in the renderer
     */
    size_t showRRT(SurfaceRenderer& renderer,
                   vtkSmartPointer<vtkPolyData> rrtSurface,
                   double maxRRT);

    // --- Surface index accessors ---

    /** @brief Get the renderer surface index for WSS (if shown) */
    [[nodiscard]] std::optional<size_t> wssIndex() const;

    /** @brief Get the renderer surface index for OSI (if shown) */
    [[nodiscard]] std::optional<size_t> osiIndex() const;

    /** @brief Get the renderer surface index for AFI (if shown) */
    [[nodiscard]] std::optional<size_t> afiIndex() const;

    /** @brief Get the renderer surface index for RRT (if shown) */
    [[nodiscard]] std::optional<size_t> rrtIndex() const;

    // --- AFI computation ---

    /**
     * @brief Compute AFI array from TAWSS surface data
     *
     * AFI = TAWSS_vertex / mean(TAWSS_all_vertices)
     * Adds an "AFI" point data array to a deep-copied output surface.
     *
     * @param tawssSurface Mesh with "TAWSS" point data array
     * @return New polydata with added "AFI" array, or nullptr if TAWSS not found
     */
    [[nodiscard]] static vtkSmartPointer<vtkPolyData>
    computeAFI(vtkSmartPointer<vtkPolyData> tawssSurface);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

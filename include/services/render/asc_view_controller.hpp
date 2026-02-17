#pragma once

#include <array>
#include <memory>

#include <vtkSmartPointer.h>
#include <vtkImageData.h>

class vtkRenderer;

namespace dicom_viewer::services {

/**
 * @brief ASC (Axial/Sagittal/Coronal) View controller for 3D overlay
 *
 * Creates three orthogonal cutting planes (axial, sagittal, coronal) in
 * a 3D renderer, each showing the corresponding resliced image as a
 * textured quad. Planes can be positioned at arbitrary slice indices
 * and toggled visible/hidden.
 *
 * Uses vtkImageSliceMapper + vtkImageSlice for each plane:
 * - Axial (Z): XY plane at specified Z slice
 * - Coronal (Y): XZ plane at specified Y slice
 * - Sagittal (X): YZ plane at specified X slice
 *
 * @trace SRS-FR-047, PRD FR-016
 */
class AscViewController {
public:
    AscViewController();
    ~AscViewController();

    // Non-copyable, movable
    AscViewController(const AscViewController&) = delete;
    AscViewController& operator=(const AscViewController&) = delete;
    AscViewController(AscViewController&&) noexcept;
    AscViewController& operator=(AscViewController&&) noexcept;

    /**
     * @brief Set the input volume data
     * @param imageData 3D volume (must have >=2 dimensions in each axis)
     */
    void setInputData(vtkSmartPointer<vtkImageData> imageData);

    /**
     * @brief Get current input data
     */
    [[nodiscard]] vtkSmartPointer<vtkImageData> getInputData() const;

    /**
     * @brief Set the VTK renderer where ASC planes will be displayed
     * @param renderer Non-owning pointer to 3D renderer
     */
    void setRenderer(vtkRenderer* renderer);

    /**
     * @brief Get current renderer
     */
    [[nodiscard]] vtkRenderer* getRenderer() const;

    // ==================== Visibility ====================

    /**
     * @brief Toggle visibility of all three ASC planes
     * @param visible True to show, false to hide
     */
    void setVisible(bool visible);

    /**
     * @brief Check if ASC planes are currently visible
     */
    [[nodiscard]] bool isVisible() const;

    // ==================== Plane Positioning ====================

    /**
     * @brief Set axial (Z) slice index
     * @param slice Z-axis slice index (0-based)
     */
    void setAxialSlice(int slice);

    /**
     * @brief Set coronal (Y) slice index
     * @param slice Y-axis slice index (0-based)
     */
    void setCoronalSlice(int slice);

    /**
     * @brief Set sagittal (X) slice index
     * @param slice X-axis slice index (0-based)
     */
    void setSagittalSlice(int slice);

    /**
     * @brief Set all three plane positions at once
     */
    void setSlicePositions(int axial, int coronal, int sagittal);

    /**
     * @brief Get current axial slice index
     */
    [[nodiscard]] int axialSlice() const;

    /**
     * @brief Get current coronal slice index
     */
    [[nodiscard]] int coronalSlice() const;

    /**
     * @brief Get current sagittal slice index
     */
    [[nodiscard]] int sagittalSlice() const;

    /**
     * @brief Get volume dimensions [X, Y, Z]
     * @return Dimensions or {0,0,0} if no data set
     */
    [[nodiscard]] std::array<int, 3> dimensions() const;

    // ==================== Rendering ====================

    /**
     * @brief Set window/level for the ASC plane display
     */
    void setWindowLevel(double width, double center);

    /**
     * @brief Get current window/level
     */
    [[nodiscard]] std::pair<double, double> getWindowLevel() const;

    /**
     * @brief Set opacity for all ASC planes
     * @param opacity Opacity value (0.0-1.0)
     */
    void setOpacity(double opacity);

    /**
     * @brief Get current opacity
     */
    [[nodiscard]] double getOpacity() const;

    /**
     * @brief Force rendering update
     */
    void update();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

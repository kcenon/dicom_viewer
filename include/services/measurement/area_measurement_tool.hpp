#pragma once

#include "measurement_types.hpp"

#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <vtkSmartPointer.h>

class vtkRenderer;
class vtkRenderWindowInteractor;

namespace dicom_viewer::services {

/**
 * @brief Area measurement tool for ROI-based area measurements
 *
 * Provides area measurement capabilities on medical images using various
 * ROI shapes: ellipse, rectangle, polygon, and freehand. Measurements are
 * displayed in mm² and cm² using DICOM pixel spacing for accurate calculations.
 *
 * Features:
 * - Ellipse ROI with adjustable semi-axes
 * - Rectangle ROI with drag handles
 * - Polygon ROI with vertex editing
 * - Freehand ROI drawing
 * - ROI copy to other slices
 * - Measurement persistence across slice changes
 *
 * @example
 * @code
 * AreaMeasurementTool tool;
 * tool.setRenderer(renderer);
 * tool.setInteractor(interactor);
 *
 * // Start rectangle ROI
 * tool.startRoiDrawing(RoiType::Rectangle);
 *
 * // Set callback for when measurement completes
 * tool.setMeasurementCompletedCallback([](const AreaMeasurement& m) {
 *     std::cout << "Area: " << m.areaMm2 << " mm²\n";
 * });
 *
 * // Get all measurements
 * auto areas = tool.getMeasurements();
 * @endcode
 *
 * @trace SRS-FR-027
 */
class AreaMeasurementTool {
public:
    /// Callback for area measurement completion
    using AreaCallback = std::function<void(const AreaMeasurement&)>;

    AreaMeasurementTool();
    ~AreaMeasurementTool();

    // Non-copyable, movable
    AreaMeasurementTool(const AreaMeasurementTool&) = delete;
    AreaMeasurementTool& operator=(const AreaMeasurementTool&) = delete;
    AreaMeasurementTool(AreaMeasurementTool&&) noexcept;
    AreaMeasurementTool& operator=(AreaMeasurementTool&&) noexcept;

    /**
     * @brief Set the renderer for measurements
     * @param renderer VTK renderer to add measurement widgets to
     */
    void setRenderer(vtkRenderer* renderer);

    /**
     * @brief Set the interactor for measurements
     * @param interactor VTK interactor for widget interaction
     */
    void setInteractor(vtkRenderWindowInteractor* interactor);

    /**
     * @brief Set pixel spacing for accurate area calculation
     * @param spacingX Pixel spacing in X direction (mm)
     * @param spacingY Pixel spacing in Y direction (mm)
     * @param spacingZ Pixel spacing in Z direction (mm)
     */
    void setPixelSpacing(double spacingX, double spacingY, double spacingZ);

    /**
     * @brief Set the current slice index for 2D measurements
     * @param sliceIndex Current slice index
     */
    void setCurrentSlice(int sliceIndex);

    /**
     * @brief Get current ROI drawing mode
     * @return Current ROI type being drawn, or nullopt if not drawing
     */
    [[nodiscard]] std::optional<RoiType> getCurrentRoiType() const noexcept;

    /**
     * @brief Start a new ROI drawing
     * @param type Type of ROI to draw
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, MeasurementError> startRoiDrawing(RoiType type);

    /**
     * @brief Cancel the current ROI drawing
     */
    void cancelCurrentRoi();

    /**
     * @brief Complete and finalize the current ROI
     */
    void completeCurrentRoi();

    /**
     * @brief Get all area measurements
     * @return Vector of area measurements
     */
    [[nodiscard]] std::vector<AreaMeasurement> getMeasurements() const;

    /**
     * @brief Get a specific area measurement by ID
     * @param id Measurement ID
     * @return Area measurement if found
     */
    [[nodiscard]] std::optional<AreaMeasurement> getMeasurement(int id) const;

    /**
     * @brief Delete an area measurement
     * @param id Measurement ID to delete
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, MeasurementError> deleteMeasurement(int id);

    /**
     * @brief Delete all measurements
     */
    void deleteAllMeasurements();

    /**
     * @brief Set visibility of an area measurement
     * @param id Measurement ID
     * @param visible Visibility state
     */
    void setMeasurementVisibility(int id, bool visible);

    /**
     * @brief Show only measurements on the specified slice
     * @param sliceIndex Slice index to show (-1 to show all)
     */
    void showMeasurementsForSlice(int sliceIndex);

    /**
     * @brief Copy ROI to another slice
     * @param measurementId Source measurement ID
     * @param targetSlice Target slice index
     * @return New measurement ID or error
     */
    [[nodiscard]] std::expected<int, MeasurementError>
    copyRoiToSlice(int measurementId, int targetSlice);

    /**
     * @brief Set display parameters
     * @param params Display parameters
     */
    void setDisplayParams(const MeasurementDisplayParams& params);

    /**
     * @brief Get current display parameters
     * @return Display parameters
     */
    [[nodiscard]] MeasurementDisplayParams getDisplayParams() const;

    /**
     * @brief Set callback for area measurement completion
     * @param callback Callback function
     */
    void setMeasurementCompletedCallback(AreaCallback callback);

    /**
     * @brief Update the label for an area measurement
     * @param id Measurement ID
     * @param label New label
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, MeasurementError>
    updateLabel(int id, const std::string& label);

    /**
     * @brief Get the total number of measurements
     * @return Total count of all measurements
     */
    [[nodiscard]] size_t getMeasurementCount() const noexcept;

    /**
     * @brief Check if a ROI drawing is currently in progress
     * @return True if drawing
     */
    [[nodiscard]] bool isDrawing() const noexcept;

    /**
     * @brief Render all visible measurements
     */
    void render();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

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
 * @file linear_measurement_tool.hpp
 * @brief Linear distance and angle measurement with VTK widget integration
 * @details Provides interactive distance and angle measurements using
 *          vtkDistanceWidget and vtkAngleWidget. Handles placement,
 *          display formatting, and physical unit conversion for
 *          DICOM-calibrated image coordinates.
 *
 * ## Thread Safety
 * - Widget interactions must occur on the main (UI) thread
 * - Measurement results are value types safe for cross-thread access
 *
 * @author kcenon
 * @since 1.0.0
 */

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
class vtkDistanceWidget;
class vtkAngleWidget;

namespace dicom_viewer::services {

/**
 * @brief Linear measurement tool for distance and angle measurements
 *
 * Provides distance and angle measurement capabilities on medical images
 * using VTK widgets. Measurements are displayed in millimeters for distance
 * and degrees for angles, using DICOM pixel spacing for accurate calculations.
 *
 * Features:
 * - Two-point distance measurement using vtkDistanceWidget
 * - Three-point angle measurement using vtkAngleWidget
 * - Cobb angle measurement for spine analysis
 * - Measurement persistence across slice changes
 * - Edit and delete existing measurements
 *
 * @example
 * @code
 * LinearMeasurementTool tool;
 * tool.setRenderer(renderer);
 * tool.setInteractor(interactor);
 *
 * // Start distance measurement
 * tool.startDistanceMeasurement();
 *
 * // Set callback for when measurement completes
 * tool.setMeasurementCompletedCallback([](const DistanceMeasurement& m) {
 *     std::cout << "Distance: " << m.distanceMm << " mm\n";
 * });
 *
 * // Get all measurements
 * auto distances = tool.getDistanceMeasurements();
 * @endcode
 *
 * @trace SRS-FR-026
 */
class LinearMeasurementTool {
public:
    /// Callback for distance measurement completion
    using DistanceCallback = std::function<void(const DistanceMeasurement&)>;

    /// Callback for angle measurement completion
    using AngleCallback = std::function<void(const AngleMeasurement&)>;

    LinearMeasurementTool();
    ~LinearMeasurementTool();

    // Non-copyable, movable
    LinearMeasurementTool(const LinearMeasurementTool&) = delete;
    LinearMeasurementTool& operator=(const LinearMeasurementTool&) = delete;
    LinearMeasurementTool(LinearMeasurementTool&&) noexcept;
    LinearMeasurementTool& operator=(LinearMeasurementTool&&) noexcept;

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
     * @brief Set pixel spacing for accurate distance calculation
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
     * @brief Get current measurement mode
     * @return Current measurement mode
     */
    [[nodiscard]] MeasurementMode getMode() const noexcept;

    /**
     * @brief Start a new distance measurement
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, MeasurementError> startDistanceMeasurement();

    /**
     * @brief Start a new angle measurement
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, MeasurementError> startAngleMeasurement();

    /**
     * @brief Start a new Cobb angle measurement
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, MeasurementError> startCobbAngleMeasurement();

    /**
     * @brief Cancel the current measurement
     */
    void cancelMeasurement();

    /**
     * @brief Complete and finalize the current measurement
     */
    void completeMeasurement();

    /**
     * @brief Get all distance measurements
     * @return Vector of distance measurements
     */
    [[nodiscard]] std::vector<DistanceMeasurement> getDistanceMeasurements() const;

    /**
     * @brief Get all angle measurements
     * @return Vector of angle measurements
     */
    [[nodiscard]] std::vector<AngleMeasurement> getAngleMeasurements() const;

    /**
     * @brief Get a specific distance measurement by ID
     * @param id Measurement ID
     * @return Distance measurement if found
     */
    [[nodiscard]] std::optional<DistanceMeasurement> getDistanceMeasurement(int id) const;

    /**
     * @brief Get a specific angle measurement by ID
     * @param id Measurement ID
     * @return Angle measurement if found
     */
    [[nodiscard]] std::optional<AngleMeasurement> getAngleMeasurement(int id) const;

    /**
     * @brief Delete a distance measurement
     * @param id Measurement ID to delete
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, MeasurementError> deleteDistanceMeasurement(int id);

    /**
     * @brief Delete an angle measurement
     * @param id Measurement ID to delete
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, MeasurementError> deleteAngleMeasurement(int id);

    /**
     * @brief Delete all measurements
     */
    void deleteAllMeasurements();

    /**
     * @brief Set visibility of a distance measurement
     * @param id Measurement ID
     * @param visible Visibility state
     */
    void setDistanceMeasurementVisibility(int id, bool visible);

    /**
     * @brief Set visibility of an angle measurement
     * @param id Measurement ID
     * @param visible Visibility state
     */
    void setAngleMeasurementVisibility(int id, bool visible);

    /**
     * @brief Show only measurements on the specified slice
     * @param sliceIndex Slice index to show (-1 to show all)
     */
    void showMeasurementsForSlice(int sliceIndex);

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
     * @brief Set callback for distance measurement completion
     * @param callback Callback function
     */
    void setDistanceCompletedCallback(DistanceCallback callback);

    /**
     * @brief Set callback for angle measurement completion
     * @param callback Callback function
     */
    void setAngleCompletedCallback(AngleCallback callback);

    /**
     * @brief Update the label for a distance measurement
     * @param id Measurement ID
     * @param label New label
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, MeasurementError>
    updateDistanceLabel(int id, const std::string& label);

    /**
     * @brief Update the label for an angle measurement
     * @param id Measurement ID
     * @param label New label
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, MeasurementError>
    updateAngleLabel(int id, const std::string& label);

    /**
     * @brief Get the total number of measurements
     * @return Total count of all measurements
     */
    [[nodiscard]] size_t getMeasurementCount() const noexcept;

    /**
     * @brief Check if a measurement is currently in progress
     * @return True if measuring
     */
    [[nodiscard]] bool isMeasuring() const noexcept;

    /**
     * @brief Render all visible measurements
     */
    void render();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

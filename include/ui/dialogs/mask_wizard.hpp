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

#pragma once

#include <memory>
#include <vector>
#include <QColor>
#include <QWizard>

namespace dicom_viewer::ui {

/**
 * @brief 3D bounding box for the Crop step
 */
struct CropRegion {
    int xMin = 0, xMax = 0;
    int yMin = 0, yMax = 0;
    int zMin = 0, zMax = 0;
};

/**
 * @brief Data for a single connected component in the Separate step
 */
struct ComponentInfo {
    int label = 0;           ///< Component label (1-based)
    int voxelCount = 0;      ///< Number of voxels in this component
    QColor color;            ///< Display color for this component
    bool selected = true;    ///< Whether this component is selected
};

/**
 * @brief Aggregated result from all wizard steps
 *
 * Provides a single snapshot of the complete wizard configuration
 * for external consumers (e.g., LabelManager integration).
 */
struct MaskWizardResult {
    CropRegion crop;                     ///< Step 1: Crop bounds
    int thresholdMin = 0;                ///< Step 2: Minimum intensity
    int thresholdMax = 0;                ///< Step 2: Maximum intensity
    std::vector<int> selectedComponents; ///< Step 3: Selected component indices (0-based)
    int referencePhase = 0;              ///< Step 4: Phase to propagate from
    int phaseCount = 1;                  ///< Step 4: Total cardiac phases
};

/**
 * @brief Wizard page identifiers for the Mask Wizard workflow
 */
enum class MaskWizardStep {
    Crop = 0,      ///< Step 1: 3D bounding box crop
    Threshold,     ///< Step 2: Intensity threshold
    Separate,      ///< Step 3: Connected component separation
    Track          ///< Step 4: Phase propagation
};

/**
 * @brief Step-by-step wizard for semi-automatic vessel segmentation
 *
 * Guides users through Cropping -> Threshold -> Separate -> Track workflow.
 * Each step builds on the previous result with clear visual feedback.
 *
 * @trace SRS-FR-023, PRD FR-015
 */
class MaskWizard : public QWizard {
    Q_OBJECT

public:
    explicit MaskWizard(QWidget* parent = nullptr);
    ~MaskWizard() override;

    // Non-copyable
    MaskWizard(const MaskWizard&) = delete;
    MaskWizard& operator=(const MaskWizard&) = delete;

    /**
     * @brief Get the current wizard step
     * @return Current step enum value
     */
    [[nodiscard]] MaskWizardStep currentStep() const;

    // -- Crop page API --

    /**
     * @brief Set volume dimensions to configure crop spinbox ranges
     * @param x Number of voxels along X axis
     * @param y Number of voxels along Y axis
     * @param z Number of voxels along Z axis
     */
    void setVolumeDimensions(int x, int y, int z);

    /**
     * @brief Get current crop region bounds
     */
    [[nodiscard]] CropRegion cropRegion() const;

    /**
     * @brief Check if crop region covers the full volume (no actual crop)
     * @return True if crop equals full volume dimensions
     */
    [[nodiscard]] bool isCropFullVolume() const;

    // -- Threshold page API --

    /**
     * @brief Configure the valid intensity range for threshold sliders
     * @param min Minimum intensity value (e.g., -1024 for CT HU)
     * @param max Maximum intensity value (e.g., 3071 for CT HU)
     */
    void setThresholdRange(int min, int max);

    /**
     * @brief Get current minimum threshold value
     */
    [[nodiscard]] int thresholdMin() const;

    /**
     * @brief Get current maximum threshold value
     */
    [[nodiscard]] int thresholdMax() const;

    /**
     * @brief Set threshold from Otsu auto-calculation result
     * @param value Otsu threshold value (sets min=value, max=range_max)
     */
    void setOtsuThreshold(double value);

    // -- Separate page API --

    /**
     * @brief Populate the component list from external analysis
     * @param components List of connected components to display
     */
    void setComponents(const std::vector<ComponentInfo>& components);

    /**
     * @brief Get the number of components
     */
    [[nodiscard]] int componentCount() const;

    /**
     * @brief Get indices of selected components (0-based)
     */
    [[nodiscard]] std::vector<int> selectedComponentIndices() const;

    // -- Track page API --

    /**
     * @brief Set the number of cardiac phases for propagation
     * @param count Total number of phases (must be >= 1)
     */
    void setPhaseCount(int count);

    /**
     * @brief Get the configured phase count
     */
    [[nodiscard]] int phaseCount() const;

    /**
     * @brief Get the selected reference phase index
     * @return 0-based reference phase index
     */
    [[nodiscard]] int referencePhase() const;

    /**
     * @brief Set the reference phase index programmatically
     * @param phase 0-based phase index (clamped to valid range)
     */
    void setReferencePhase(int phase);

    /**
     * @brief Update the propagation progress bar
     * @param percent Progress value (0-100)
     */
    void setTrackProgress(int percent);

    /**
     * @brief Update the track page status message
     * @param status Status text to display
     */
    void setTrackStatus(const QString& status);

    /**
     * @brief Get aggregated result from all wizard steps
     * @return Snapshot of all wizard parameters
     */
    [[nodiscard]] MaskWizardResult wizardResult() const;

signals:
    /**
     * @brief Emitted when the wizard completes all steps successfully
     */
    void wizardCompleted();

    /**
     * @brief Emitted when the wizard completes with aggregated result
     * @param result Snapshot of all wizard step parameters
     */
    void wizardFinished(const MaskWizardResult& result);

    /**
     * @brief Emitted when threshold slider values change
     */
    void thresholdChanged(int min, int max);

    /**
     * @brief Emitted when user clicks the Otsu auto-threshold button
     */
    void otsuRequested();

    /**
     * @brief Emitted when component selection changes in the Separate step
     */
    void componentSelectionChanged();

    /**
     * @brief Emitted when crop region bounds change
     */
    void cropRegionChanged();

    /**
     * @brief Emitted when user clicks the Run Propagation button
     */
    void propagationRequested();

    /**
     * @brief Emitted when the reference phase selection changes
     * @param phase New reference phase index
     */
    void referencePhaseChanged(int phase);

private:
    void setupPages();
    void setupAppearance();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

Q_DECLARE_METATYPE(dicom_viewer::ui::MaskWizardResult)

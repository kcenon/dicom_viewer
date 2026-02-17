#pragma once

#include <memory>
#include <QWizard>

namespace dicom_viewer::ui {

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

signals:
    /**
     * @brief Emitted when the wizard completes all steps successfully
     */
    void wizardCompleted();

private:
    void setupPages();
    void setupAppearance();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

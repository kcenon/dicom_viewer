#pragma once

#include "ui/dialogs/mask_wizard.hpp"
#include "services/segmentation/threshold_segmenter.hpp"
#include "services/segmentation/phase_tracker.hpp"

#include <cstdint>
#include <memory>
#include <vector>

#include <itkImage.h>
#include <QObject>

namespace dicom_viewer::services {
class LabelManager;
}

namespace dicom_viewer::ui {

/**
 * @brief Coordinates MaskWizard UI with segmentation service layer
 *
 * Wires the 4-step wizard (Crop -> Threshold -> Separate -> Track) to:
 * - ThresholdSegmenter for manual/Otsu thresholding (Step 2)
 * - ConnectedComponentImageFilter for component analysis (Step 3)
 * - PhaseTracker for temporal mask propagation (Step 4)
 * - LabelManager for output label creation
 *
 * Long-running operations (connected component, propagation) are run
 * asynchronously via QtConcurrent to keep the UI responsive.
 *
 * @trace SRS-FR-023, SRS-FR-047
 */
class MaskWizardController : public QObject {
    Q_OBJECT

public:
    /// Label map type (matches LabelManager::LabelMapType)
    using LabelMapType = itk::Image<uint8_t, 3>;

    /**
     * @brief Input context for the wizard workflow
     */
    struct Context {
        /// Source image from VTK->ITK conversion
        services::ThresholdSegmenter::ImageType::Pointer sourceImage;

        /// Magnitude images for all cardiac phases (for Step 4)
        std::vector<services::PhaseTracker::FloatImage3D::Pointer> magnitudePhases;

        /// Current phase index in the temporal navigator
        int currentPhase = 0;
    };

    /**
     * @brief Construct controller and wire wizard signals
     *
     * @param wizard The MaskWizard to control (non-owning, caller manages lifetime)
     * @param parent QObject parent for lifetime management
     */
    explicit MaskWizardController(MaskWizard* wizard, QObject* parent = nullptr);
    ~MaskWizardController() override;

    MaskWizardController(const MaskWizardController&) = delete;
    MaskWizardController& operator=(const MaskWizardController&) = delete;

    /**
     * @brief Set the input context (source image and phase data)
     */
    void setContext(const Context& context);

    /**
     * @brief Set the label manager for output creation
     * @param manager Non-owning pointer (caller manages lifetime)
     */
    void setLabelManager(services::LabelManager* manager);

signals:
    /**
     * @brief Emitted when the final mask is ready for viewport display
     */
    void maskCreated(LabelMapType::Pointer mask);

    /**
     * @brief Emitted when an error occurs during processing
     */
    void errorOccurred(const QString& message);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

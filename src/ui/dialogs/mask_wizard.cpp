#include "ui/dialogs/mask_wizard.hpp"

#include <QLabel>
#include <QVBoxLayout>
#include <QWizardPage>

namespace dicom_viewer::ui {

namespace {

/**
 * @brief Base wizard page with step title and placeholder content
 */
class StepPage : public QWizardPage {
public:
    StepPage(const QString& title, const QString& subtitle,
             const QString& description, QWidget* parent = nullptr)
        : QWizardPage(parent)
    {
        setTitle(title);
        setSubTitle(subtitle);

        auto* layout = new QVBoxLayout(this);

        auto* descLabel = new QLabel(description, this);
        descLabel->setWordWrap(true);
        descLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        layout->addWidget(descLabel);

        layout->addStretch();
    }
};

}  // anonymous namespace

class MaskWizard::Impl {
public:
    StepPage* cropPage = nullptr;
    StepPage* thresholdPage = nullptr;
    StepPage* separatePage = nullptr;
    StepPage* trackPage = nullptr;
};

MaskWizard::MaskWizard(QWidget* parent)
    : QWizard(parent)
    , impl_(std::make_unique<Impl>())
{
    setupPages();
    setupAppearance();

    connect(this, &QWizard::accepted, this, &MaskWizard::wizardCompleted);
}

MaskWizard::~MaskWizard() = default;

void MaskWizard::setupPages()
{
    impl_->cropPage = new StepPage(
        tr("Step 1: Crop"),
        tr("Define a 3D bounding box to reduce the region of interest"),
        tr("Use the handles on each plane (Axial, Coronal, Sagittal) to "
           "define the crop region. Only the cropped volume will be used "
           "in subsequent steps.\n\n"
           "Note: Cropping is irreversible within this wizard session."),
        this);

    impl_->thresholdPage = new StepPage(
        tr("Step 2: Threshold"),
        tr("Set intensity range to create a binary mask"),
        tr("Adjust the minimum and maximum intensity sliders to isolate "
           "the structures of interest. The histogram shows the voxel "
           "intensity distribution of the cropped volume.\n\n"
           "A real-time preview overlay shows the resulting mask."),
        this);

    impl_->separatePage = new StepPage(
        tr("Step 3: Separate"),
        tr("Select connected components to keep"),
        tr("The binary mask is analyzed for connected regions. Each region "
           "is shown with a unique color. Click on regions to select or "
           "deselect them.\n\n"
           "Only selected components will proceed to the next step."),
        this);

    impl_->trackPage = new StepPage(
        tr("Step 4: Track"),
        tr("Propagate the mask to all cardiac phases"),
        tr("The segmentation from the current phase will be propagated "
           "to all other cardiac phases using temporal tracking.\n\n"
           "Click Finish to accept the result and add it to the Mask Manager."),
        this);

    addPage(impl_->cropPage);
    addPage(impl_->thresholdPage);
    addPage(impl_->separatePage);
    addPage(impl_->trackPage);
}

void MaskWizard::setupAppearance()
{
    setWindowTitle(tr("Mask Wizard"));
    setMinimumSize(500, 400);
    setWizardStyle(QWizard::ModernStyle);

    setButtonText(QWizard::BackButton, tr("< Back"));
    setButtonText(QWizard::NextButton, tr("Next >"));
    setButtonText(QWizard::CancelButton, tr("Cancel"));
    setButtonText(QWizard::FinishButton, tr("Finish"));
}

MaskWizardStep MaskWizard::currentStep() const
{
    return static_cast<MaskWizardStep>(currentId());
}

} // namespace dicom_viewer::ui

#include "ui/dialogs/mask_wizard.hpp"

#include <cmath>

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWizardPage>

namespace dicom_viewer::ui {

namespace {

constexpr int kDefaultRangeMin = -1024;
constexpr int kDefaultRangeMax = 3071;

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

/**
 * @brief Functional threshold page with dual sliders and Otsu button
 */
class ThresholdPage : public QWizardPage {
public:
    explicit ThresholdPage(MaskWizard* wizard, QWidget* parent = nullptr)
        : QWizardPage(parent)
        , wizard_(wizard)
    {
        setTitle(tr("Step 2: Threshold"));
        setSubTitle(tr("Set intensity range to create a binary mask"));

        auto* layout = new QVBoxLayout(this);

        auto* descLabel = new QLabel(
            tr("Adjust the minimum and maximum intensity sliders to isolate "
               "the structures of interest."),
            this);
        descLabel->setWordWrap(true);
        layout->addWidget(descLabel);

        layout->addSpacing(12);

        // Min threshold row
        layout->addWidget(new QLabel(tr("Min Threshold:"), this));
        auto* minRow = new QHBoxLayout();
        minSlider_ = new QSlider(Qt::Horizontal, this);
        minSpin_ = new QSpinBox(this);
        minRow->addWidget(minSlider_, 1);
        minRow->addWidget(minSpin_);
        layout->addLayout(minRow);

        layout->addSpacing(8);

        // Max threshold row
        layout->addWidget(new QLabel(tr("Max Threshold:"), this));
        auto* maxRow = new QHBoxLayout();
        maxSlider_ = new QSlider(Qt::Horizontal, this);
        maxSpin_ = new QSpinBox(this);
        maxRow->addWidget(maxSlider_, 1);
        maxRow->addWidget(maxSpin_);
        layout->addLayout(maxRow);

        layout->addSpacing(12);

        // Otsu button
        auto* buttonRow = new QHBoxLayout();
        otsuButton_ = new QPushButton(tr("Auto (Otsu)"), this);
        buttonRow->addWidget(otsuButton_);
        buttonRow->addStretch();
        layout->addLayout(buttonRow);

        layout->addSpacing(8);

        // Status label
        statusLabel_ = new QLabel(tr("Ready"), this);
        statusLabel_->setStyleSheet("color: gray; font-style: italic;");
        layout->addWidget(statusLabel_);

        layout->addStretch();

        setRange(kDefaultRangeMin, kDefaultRangeMax);
        setThresholdValues(kDefaultRangeMin, kDefaultRangeMax);
        setupConnections();
    }

    void setRange(int min, int max) {
        minSlider_->setRange(min, max);
        maxSlider_->setRange(min, max);
        minSpin_->setRange(min, max);
        maxSpin_->setRange(min, max);
    }

    void setThresholdValues(int minVal, int maxVal) {
        blockSignals_ = true;
        minSlider_->setValue(minVal);
        minSpin_->setValue(minVal);
        maxSlider_->setValue(maxVal);
        maxSpin_->setValue(maxVal);
        blockSignals_ = false;
    }

    [[nodiscard]] int thresholdMin() const { return minSpin_->value(); }
    [[nodiscard]] int thresholdMax() const { return maxSpin_->value(); }

    void setStatusText(const QString& text) {
        statusLabel_->setText(text);
    }

private:
    void setupConnections() {
        // Min slider <-> spinbox sync
        connect(minSlider_, &QSlider::valueChanged, this, [this](int val) {
            if (blockSignals_) return;
            blockSignals_ = true;
            minSpin_->setValue(val);
            // Enforce min <= max
            if (val > maxSlider_->value()) {
                maxSlider_->setValue(val);
                maxSpin_->setValue(val);
            }
            blockSignals_ = false;
            emitThresholdChanged();
        });
        connect(minSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
            if (blockSignals_) return;
            blockSignals_ = true;
            minSlider_->setValue(val);
            if (val > maxSpin_->value()) {
                maxSlider_->setValue(val);
                maxSpin_->setValue(val);
            }
            blockSignals_ = false;
            emitThresholdChanged();
        });

        // Max slider <-> spinbox sync
        connect(maxSlider_, &QSlider::valueChanged, this, [this](int val) {
            if (blockSignals_) return;
            blockSignals_ = true;
            maxSpin_->setValue(val);
            if (val < minSlider_->value()) {
                minSlider_->setValue(val);
                minSpin_->setValue(val);
            }
            blockSignals_ = false;
            emitThresholdChanged();
        });
        connect(maxSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
            if (blockSignals_) return;
            blockSignals_ = true;
            maxSlider_->setValue(val);
            if (val < minSpin_->value()) {
                minSlider_->setValue(val);
                minSpin_->setValue(val);
            }
            blockSignals_ = false;
            emitThresholdChanged();
        });

        // Otsu button
        connect(otsuButton_, &QPushButton::clicked, this, [this]() {
            emit wizard_->otsuRequested();
        });
    }

    void emitThresholdChanged() {
        emit wizard_->thresholdChanged(minSpin_->value(), maxSpin_->value());
    }

    MaskWizard* wizard_ = nullptr;
    QSlider* minSlider_ = nullptr;
    QSlider* maxSlider_ = nullptr;
    QSpinBox* minSpin_ = nullptr;
    QSpinBox* maxSpin_ = nullptr;
    QPushButton* otsuButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    bool blockSignals_ = false;
};

}  // anonymous namespace

class MaskWizard::Impl {
public:
    StepPage* cropPage = nullptr;
    ThresholdPage* thresholdPage = nullptr;
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

    impl_->thresholdPage = new ThresholdPage(this, this);

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

void MaskWizard::setThresholdRange(int min, int max)
{
    impl_->thresholdPage->setRange(min, max);
}

int MaskWizard::thresholdMin() const
{
    return impl_->thresholdPage->thresholdMin();
}

int MaskWizard::thresholdMax() const
{
    return impl_->thresholdPage->thresholdMax();
}

void MaskWizard::setOtsuThreshold(double value)
{
    auto intValue = static_cast<int>(std::round(value));
    impl_->thresholdPage->setThresholdValues(intValue, impl_->thresholdPage->thresholdMax());
    impl_->thresholdPage->setStatusText(
        tr("Otsu threshold: %1").arg(intValue));
}

} // namespace dicom_viewer::ui

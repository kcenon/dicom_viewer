#include "ui/widgets/sp_mode_toggle.hpp"

#include <QHBoxLayout>
#include <QPushButton>

namespace dicom_viewer::ui {

class SPModeToggle::Impl {
public:
    QPushButton* sliceButton = nullptr;
    QPushButton* phaseButton = nullptr;
    ScrollMode currentMode = ScrollMode::Slice;
    bool updatingFromExternal = false;
};

SPModeToggle::SPModeToggle(QWidget* parent)
    : QWidget(parent)
    , impl_(std::make_unique<Impl>())
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    impl_->sliceButton = new QPushButton(tr("S"), this);
    impl_->phaseButton = new QPushButton(tr("P"), this);

    // Fixed size for compact toggle buttons
    const int buttonSize = 28;
    impl_->sliceButton->setFixedSize(buttonSize, buttonSize);
    impl_->phaseButton->setFixedSize(buttonSize, buttonSize);

    impl_->sliceButton->setCheckable(true);
    impl_->phaseButton->setCheckable(true);
    impl_->sliceButton->setChecked(true);

    impl_->sliceButton->setToolTip(tr("Slice mode: scroll wheel navigates slices"));
    impl_->phaseButton->setToolTip(tr("Phase mode: scroll wheel navigates cardiac phases"));

    // Styling for toggle appearance
    const QString checkedStyle =
        "QPushButton { font-weight: bold; background-color: #2a82da; "
        "color: white; border: 1px solid #1a72ca; border-radius: 3px; }"
        "QPushButton:hover { background-color: #3a92ea; }";
    const QString uncheckedStyle =
        "QPushButton { background-color: #404040; color: #aaaaaa; "
        "border: 1px solid #505050; border-radius: 3px; }"
        "QPushButton:hover { background-color: #505050; color: white; }";

    auto updateStyles = [this, checkedStyle, uncheckedStyle]() {
        impl_->sliceButton->setStyleSheet(
            impl_->currentMode == ScrollMode::Slice ? checkedStyle : uncheckedStyle);
        impl_->phaseButton->setStyleSheet(
            impl_->currentMode == ScrollMode::Phase ? checkedStyle : uncheckedStyle);
    };

    updateStyles();

    connect(impl_->sliceButton, &QPushButton::clicked, this, [this, updateStyles]() {
        if (impl_->updatingFromExternal) return;
        impl_->currentMode = ScrollMode::Slice;
        impl_->sliceButton->setChecked(true);
        impl_->phaseButton->setChecked(false);
        updateStyles();
        emit modeChanged(ScrollMode::Slice);
    });

    connect(impl_->phaseButton, &QPushButton::clicked, this, [this, updateStyles]() {
        if (impl_->updatingFromExternal) return;
        impl_->currentMode = ScrollMode::Phase;
        impl_->sliceButton->setChecked(false);
        impl_->phaseButton->setChecked(true);
        updateStyles();
        emit modeChanged(ScrollMode::Phase);
    });

    layout->addWidget(impl_->sliceButton);
    layout->addWidget(impl_->phaseButton);
}

SPModeToggle::~SPModeToggle() = default;

ScrollMode SPModeToggle::mode() const
{
    return impl_->currentMode;
}

void SPModeToggle::setMode(ScrollMode mode)
{
    if (impl_->currentMode == mode) return;

    impl_->updatingFromExternal = true;
    impl_->currentMode = mode;
    impl_->sliceButton->setChecked(mode == ScrollMode::Slice);
    impl_->phaseButton->setChecked(mode == ScrollMode::Phase);

    // Update styles
    const QString checkedStyle =
        "QPushButton { font-weight: bold; background-color: #2a82da; "
        "color: white; border: 1px solid #1a72ca; border-radius: 3px; }"
        "QPushButton:hover { background-color: #3a92ea; }";
    const QString uncheckedStyle =
        "QPushButton { background-color: #404040; color: #aaaaaa; "
        "border: 1px solid #505050; border-radius: 3px; }"
        "QPushButton:hover { background-color: #505050; color: white; }";

    impl_->sliceButton->setStyleSheet(
        mode == ScrollMode::Slice ? checkedStyle : uncheckedStyle);
    impl_->phaseButton->setStyleSheet(
        mode == ScrollMode::Phase ? checkedStyle : uncheckedStyle);

    impl_->updatingFromExternal = false;
}

} // namespace dicom_viewer::ui

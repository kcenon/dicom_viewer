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

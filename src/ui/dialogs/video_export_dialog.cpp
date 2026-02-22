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

#include "ui/dialogs/video_export_dialog.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace dicom_viewer::ui {

// =========================================================================
// Pimpl
// =========================================================================

class VideoExportDialog::Impl {
public:
    int totalPhases = 0;

    // Common controls
    QComboBox* modeCombo = nullptr;
    QLineEdit* outputPathEdit = nullptr;
    QPushButton* browseButton = nullptr;
    QComboBox* resolutionCombo = nullptr;
    QSpinBox* fpsSpin = nullptr;

    // Mode-specific stacked widget
    QStackedWidget* modeStack = nullptr;

    // Cine2D controls
    QSpinBox* cineStartPhaseSpin = nullptr;
    QSpinBox* cineEndPhaseSpin = nullptr;
    QSpinBox* cineLoopsSpin = nullptr;
    QSpinBox* cineFramesPerPhaseSpin = nullptr;

    // Rotation3D controls
    QDoubleSpinBox* rotStartAngleSpin = nullptr;
    QDoubleSpinBox* rotEndAngleSpin = nullptr;
    QDoubleSpinBox* rotElevationSpin = nullptr;
    QSpinBox* rotTotalFramesSpin = nullptr;

    // Combined controls
    QDoubleSpinBox* combStartAngleSpin = nullptr;
    QDoubleSpinBox* combEndAngleSpin = nullptr;
    QDoubleSpinBox* combElevationSpin = nullptr;
    QSpinBox* combPhaseLoopsSpin = nullptr;
    QSpinBox* combFramesPerPhaseSpin = nullptr;

    QDialogButtonBox* buttonBox = nullptr;

    struct Resolution {
        int width;
        int height;
        const char* label;
    };

    static constexpr Resolution kResolutions[] = {
        {1920, 1080, "1920 x 1080 (Full HD)"},
        {1280,  720, "1280 x 720 (HD)"},
        { 854,  480, "854 x 480 (480p)"},
        { 640,  360, "640 x 360 (360p)"},
    };

    std::pair<int, int> selectedResolution() const {
        int idx = resolutionCombo->currentIndex();
        if (idx >= 0 && idx < static_cast<int>(std::size(kResolutions))) {
            return {kResolutions[idx].width, kResolutions[idx].height};
        }
        return {1920, 1080};
    }
};

// =========================================================================
// Construction
// =========================================================================

VideoExportDialog::VideoExportDialog(int totalPhases, QWidget* parent)
    : QDialog(parent)
    , impl_(std::make_unique<Impl>())
{
    impl_->totalPhases = totalPhases;
    setWindowTitle(tr("Export Video"));
    setMinimumWidth(480);
    setupUI();
    setupConnections();
    updateModeOptions();
}

VideoExportDialog::~VideoExportDialog() = default;

// =========================================================================
// Setup
// =========================================================================

void VideoExportDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    // --- Common settings ---
    auto* commonGroup = new QGroupBox(tr("General Settings"));
    auto* commonForm = new QFormLayout(commonGroup);

    impl_->modeCombo = new QComboBox;
    impl_->modeCombo->addItem(tr("2D Cine Phase Animation"), static_cast<int>(ExportMode::Cine2D));
    impl_->modeCombo->addItem(tr("3D Rotation"), static_cast<int>(ExportMode::Rotation3D));
    impl_->modeCombo->addItem(tr("3D Combined (Rotation + Phase)"), static_cast<int>(ExportMode::Combined));

    // Disable cine modes if no temporal data
    if (impl_->totalPhases < 1) {
        impl_->modeCombo->model()->setData(
            impl_->modeCombo->model()->index(0, 0), false, Qt::UserRole - 1);
        impl_->modeCombo->model()->setData(
            impl_->modeCombo->model()->index(2, 0), false, Qt::UserRole - 1);
        impl_->modeCombo->setCurrentIndex(1);  // Default to 3D Rotation
    }

    commonForm->addRow(tr("Export Mode:"), impl_->modeCombo);

    // Output path
    auto* pathLayout = new QHBoxLayout;
    impl_->outputPathEdit = new QLineEdit;
    impl_->outputPathEdit->setPlaceholderText(tr("Select output file..."));
    impl_->browseButton = new QPushButton(tr("Browse..."));
    pathLayout->addWidget(impl_->outputPathEdit);
    pathLayout->addWidget(impl_->browseButton);
    commonForm->addRow(tr("Output File:"), pathLayout);

    // Resolution
    impl_->resolutionCombo = new QComboBox;
    for (const auto& res : Impl::kResolutions) {
        impl_->resolutionCombo->addItem(res.label);
    }
    commonForm->addRow(tr("Resolution:"), impl_->resolutionCombo);

    // FPS
    impl_->fpsSpin = new QSpinBox;
    impl_->fpsSpin->setRange(1, 120);
    impl_->fpsSpin->setValue(15);
    impl_->fpsSpin->setSuffix(tr(" fps"));
    commonForm->addRow(tr("Frame Rate:"), impl_->fpsSpin);

    mainLayout->addWidget(commonGroup);

    // --- Mode-specific settings ---
    impl_->modeStack = new QStackedWidget;

    // Page 0: Cine2D
    auto* cinePage = new QGroupBox(tr("Cine Settings"));
    auto* cineForm = new QFormLayout(cinePage);

    impl_->cineStartPhaseSpin = new QSpinBox;
    impl_->cineStartPhaseSpin->setRange(0, std::max(0, impl_->totalPhases - 1));
    impl_->cineStartPhaseSpin->setValue(0);
    cineForm->addRow(tr("Start Phase:"), impl_->cineStartPhaseSpin);

    impl_->cineEndPhaseSpin = new QSpinBox;
    impl_->cineEndPhaseSpin->setRange(-1, std::max(0, impl_->totalPhases - 1));
    impl_->cineEndPhaseSpin->setValue(-1);
    impl_->cineEndPhaseSpin->setSpecialValueText(tr("Last"));
    cineForm->addRow(tr("End Phase:"), impl_->cineEndPhaseSpin);

    impl_->cineLoopsSpin = new QSpinBox;
    impl_->cineLoopsSpin->setRange(1, 10);
    impl_->cineLoopsSpin->setValue(1);
    cineForm->addRow(tr("Loops:"), impl_->cineLoopsSpin);

    impl_->cineFramesPerPhaseSpin = new QSpinBox;
    impl_->cineFramesPerPhaseSpin->setRange(1, 10);
    impl_->cineFramesPerPhaseSpin->setValue(1);
    cineForm->addRow(tr("Frames per Phase:"), impl_->cineFramesPerPhaseSpin);

    impl_->modeStack->addWidget(cinePage);

    // Page 1: Rotation3D
    auto* rotPage = new QGroupBox(tr("Rotation Settings"));
    auto* rotForm = new QFormLayout(rotPage);

    impl_->rotStartAngleSpin = new QDoubleSpinBox;
    impl_->rotStartAngleSpin->setRange(-720.0, 720.0);
    impl_->rotStartAngleSpin->setValue(0.0);
    impl_->rotStartAngleSpin->setSuffix(tr("\u00B0"));
    rotForm->addRow(tr("Start Angle:"), impl_->rotStartAngleSpin);

    impl_->rotEndAngleSpin = new QDoubleSpinBox;
    impl_->rotEndAngleSpin->setRange(-720.0, 720.0);
    impl_->rotEndAngleSpin->setValue(360.0);
    impl_->rotEndAngleSpin->setSuffix(tr("\u00B0"));
    rotForm->addRow(tr("End Angle:"), impl_->rotEndAngleSpin);

    impl_->rotElevationSpin = new QDoubleSpinBox;
    impl_->rotElevationSpin->setRange(-90.0, 90.0);
    impl_->rotElevationSpin->setValue(15.0);
    impl_->rotElevationSpin->setSuffix(tr("\u00B0"));
    rotForm->addRow(tr("Elevation:"), impl_->rotElevationSpin);

    impl_->rotTotalFramesSpin = new QSpinBox;
    impl_->rotTotalFramesSpin->setRange(2, 3600);
    impl_->rotTotalFramesSpin->setValue(180);
    rotForm->addRow(tr("Total Frames:"), impl_->rotTotalFramesSpin);

    impl_->modeStack->addWidget(rotPage);

    // Page 2: Combined
    auto* combPage = new QGroupBox(tr("Combined Settings"));
    auto* combForm = new QFormLayout(combPage);

    impl_->combStartAngleSpin = new QDoubleSpinBox;
    impl_->combStartAngleSpin->setRange(-720.0, 720.0);
    impl_->combStartAngleSpin->setValue(0.0);
    impl_->combStartAngleSpin->setSuffix(tr("\u00B0"));
    combForm->addRow(tr("Start Angle:"), impl_->combStartAngleSpin);

    impl_->combEndAngleSpin = new QDoubleSpinBox;
    impl_->combEndAngleSpin->setRange(-720.0, 720.0);
    impl_->combEndAngleSpin->setValue(360.0);
    impl_->combEndAngleSpin->setSuffix(tr("\u00B0"));
    combForm->addRow(tr("End Angle:"), impl_->combEndAngleSpin);

    impl_->combElevationSpin = new QDoubleSpinBox;
    impl_->combElevationSpin->setRange(-90.0, 90.0);
    impl_->combElevationSpin->setValue(15.0);
    impl_->combElevationSpin->setSuffix(tr("\u00B0"));
    combForm->addRow(tr("Elevation:"), impl_->combElevationSpin);

    impl_->combPhaseLoopsSpin = new QSpinBox;
    impl_->combPhaseLoopsSpin->setRange(1, 10);
    impl_->combPhaseLoopsSpin->setValue(1);
    combForm->addRow(tr("Phase Loops:"), impl_->combPhaseLoopsSpin);

    impl_->combFramesPerPhaseSpin = new QSpinBox;
    impl_->combFramesPerPhaseSpin->setRange(1, 10);
    impl_->combFramesPerPhaseSpin->setValue(1);
    combForm->addRow(tr("Frames per Phase:"), impl_->combFramesPerPhaseSpin);

    impl_->modeStack->addWidget(combPage);

    mainLayout->addWidget(impl_->modeStack);

    // --- Button box ---
    impl_->buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(impl_->buttonBox);
}

void VideoExportDialog::setupConnections()
{
    connect(impl_->modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VideoExportDialog::onModeChanged);
    connect(impl_->browseButton, &QPushButton::clicked,
            this, &VideoExportDialog::onBrowseOutput);
    connect(impl_->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(impl_->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void VideoExportDialog::updateModeOptions()
{
    int idx = impl_->modeCombo->currentIndex();
    impl_->modeStack->setCurrentIndex(idx);

    // Set default FPS based on mode
    if (idx == 0) {
        impl_->fpsSpin->setValue(15);   // Cine: slower
    } else {
        impl_->fpsSpin->setValue(30);   // Rotation/Combined: smoother
    }
}

// =========================================================================
// Slots
// =========================================================================

void VideoExportDialog::onModeChanged(int /*index*/)
{
    updateModeOptions();
}

void VideoExportDialog::onBrowseOutput()
{
    QString filePath = QFileDialog::getSaveFileName(
        this, tr("Save Video"), QString(),
        tr("OGG Theora Video (*.ogv);;All Files (*)"));
    if (!filePath.isEmpty()) {
        impl_->outputPathEdit->setText(filePath);
    }
}

// =========================================================================
// Accessors
// =========================================================================

VideoExportDialog::ExportMode VideoExportDialog::exportMode() const
{
    return static_cast<ExportMode>(
        impl_->modeCombo->currentData().toInt());
}

std::filesystem::path VideoExportDialog::outputPath() const
{
    return std::filesystem::path(
        impl_->outputPathEdit->text().toStdString());
}

services::VideoExporter::CineConfig
VideoExportDialog::buildCineConfig() const
{
    auto [w, h] = impl_->selectedResolution();
    services::VideoExporter::CineConfig config;
    config.outputPath = outputPath();
    config.width = w;
    config.height = h;
    config.fps = impl_->fpsSpin->value();
    config.startPhase = impl_->cineStartPhaseSpin->value();
    config.endPhase = impl_->cineEndPhaseSpin->value();
    config.totalPhases = impl_->totalPhases;
    config.loops = impl_->cineLoopsSpin->value();
    config.framesPerPhase = impl_->cineFramesPerPhaseSpin->value();
    return config;
}

services::VideoExporter::RotationConfig
VideoExportDialog::buildRotationConfig() const
{
    auto [w, h] = impl_->selectedResolution();
    services::VideoExporter::RotationConfig config;
    config.outputPath = outputPath();
    config.width = w;
    config.height = h;
    config.fps = impl_->fpsSpin->value();
    config.startAngle = impl_->rotStartAngleSpin->value();
    config.endAngle = impl_->rotEndAngleSpin->value();
    config.elevation = impl_->rotElevationSpin->value();
    config.totalFrames = impl_->rotTotalFramesSpin->value();
    return config;
}

services::VideoExporter::CombinedConfig
VideoExportDialog::buildCombinedConfig() const
{
    auto [w, h] = impl_->selectedResolution();
    services::VideoExporter::CombinedConfig config;
    config.outputPath = outputPath();
    config.width = w;
    config.height = h;
    config.fps = impl_->fpsSpin->value();
    config.startAngle = impl_->combStartAngleSpin->value();
    config.endAngle = impl_->combEndAngleSpin->value();
    config.elevation = impl_->combElevationSpin->value();
    config.totalPhases = impl_->totalPhases;
    config.phaseLoops = impl_->combPhaseLoopsSpin->value();
    config.framesPerPhase = impl_->combFramesPerPhaseSpin->value();
    return config;
}

} // namespace dicom_viewer::ui

#include "ui/viewport_layout_manager.hpp"
#include "ui/viewport_widget.hpp"

#include <QGridLayout>
#include <QLabel>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace dicom_viewer::ui {

class ViewportLayoutManager::Impl {
public:
    LayoutMode mode = LayoutMode::Single;
    QStackedWidget* stack = nullptr;

    // Layout containers (one per mode)
    QWidget* singleContainer = nullptr;
    QSplitter* dualContainer = nullptr;
    QWidget* quadContainer = nullptr;

    // Viewports: max 4 (Axial, Sagittal, Coronal, 3D)
    // Index 0 is always the "primary" viewport.
    ViewportWidget* viewports[4] = {};
    bool layoutBuilt[3] = {};  // Track which layouts have been built
};

ViewportLayoutManager::ViewportLayoutManager(QWidget* parent)
    : QWidget(parent)
    , impl_(std::make_unique<Impl>())
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    impl_->stack = new QStackedWidget(this);
    layout->addWidget(impl_->stack);

    // Build the default (Single) layout immediately
    buildSingleLayout();
    impl_->stack->setCurrentIndex(0);
}

ViewportLayoutManager::~ViewportLayoutManager() = default;

LayoutMode ViewportLayoutManager::layoutMode() const
{
    return impl_->mode;
}

ViewportWidget* ViewportLayoutManager::primaryViewport() const
{
    return impl_->viewports[0];
}

ViewportWidget* ViewportLayoutManager::viewport(int index) const
{
    if (index < 0 || index >= viewportCount()) return nullptr;
    return impl_->viewports[index];
}

int ViewportLayoutManager::viewportCount() const
{
    switch (impl_->mode) {
        case LayoutMode::Single:    return 1;
        case LayoutMode::DualSplit: return 2;
        case LayoutMode::QuadSplit: return 4;
    }
    return 1;
}

void ViewportLayoutManager::setLayoutMode(LayoutMode mode)
{
    if (impl_->mode == mode) return;
    impl_->mode = mode;

    switch (mode) {
        case LayoutMode::Single:
            if (!impl_->layoutBuilt[0]) buildSingleLayout();
            impl_->stack->setCurrentIndex(0);
            break;
        case LayoutMode::DualSplit:
            if (!impl_->layoutBuilt[1]) buildDualLayout();
            impl_->stack->setCurrentIndex(1);
            break;
        case LayoutMode::QuadSplit:
            if (!impl_->layoutBuilt[2]) buildQuadLayout();
            impl_->stack->setCurrentIndex(2);
            break;
    }

    emit layoutModeChanged(mode);
}

void ViewportLayoutManager::buildSingleLayout()
{
    impl_->singleContainer = new QWidget(impl_->stack);
    auto* layout = new QVBoxLayout(impl_->singleContainer);
    layout->setContentsMargins(0, 0, 0, 0);

    // Viewport 0 is always the primary, shared across all modes
    if (!impl_->viewports[0]) {
        impl_->viewports[0] = new ViewportWidget(impl_->singleContainer);
    }
    impl_->viewports[0]->setParent(impl_->singleContainer);
    layout->addWidget(impl_->viewports[0]);

    impl_->stack->addWidget(impl_->singleContainer);
    impl_->layoutBuilt[0] = true;
}

void ViewportLayoutManager::buildDualLayout()
{
    impl_->dualContainer = new QSplitter(Qt::Horizontal, impl_->stack);
    impl_->dualContainer->setHandleWidth(3);

    // Left: 2D slice (reuse or create viewport 0)
    if (!impl_->viewports[0]) {
        impl_->viewports[0] = new ViewportWidget(impl_->dualContainer);
    }
    impl_->viewports[0]->setParent(impl_->dualContainer);
    impl_->dualContainer->addWidget(impl_->viewports[0]);

    // Right: 3D view
    if (!impl_->viewports[1]) {
        impl_->viewports[1] = new ViewportWidget(impl_->dualContainer);
        impl_->viewports[1]->setMode(ViewportMode::VolumeRendering);
    }
    impl_->dualContainer->addWidget(impl_->viewports[1]);

    // Equal split by default
    impl_->dualContainer->setSizes({1, 1});

    impl_->stack->addWidget(impl_->dualContainer);
    impl_->layoutBuilt[1] = true;
}

void ViewportLayoutManager::buildQuadLayout()
{
    impl_->quadContainer = new QWidget(impl_->stack);
    auto* grid = new QGridLayout(impl_->quadContainer);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(2);

    // Create viewports for each quadrant if not already present
    // 0: Axial (top-left)
    if (!impl_->viewports[0]) {
        impl_->viewports[0] = new ViewportWidget(impl_->quadContainer);
    }
    impl_->viewports[0]->setParent(impl_->quadContainer);
    impl_->viewports[0]->setSliceOrientation(SliceOrientation::Axial);

    // 1: Sagittal (top-right)
    if (!impl_->viewports[1]) {
        impl_->viewports[1] = new ViewportWidget(impl_->quadContainer);
    }
    impl_->viewports[1]->setParent(impl_->quadContainer);
    impl_->viewports[1]->setSliceOrientation(SliceOrientation::Sagittal);

    // 2: Coronal (bottom-left)
    if (!impl_->viewports[2]) {
        impl_->viewports[2] = new ViewportWidget(impl_->quadContainer);
    }
    impl_->viewports[2]->setParent(impl_->quadContainer);
    impl_->viewports[2]->setSliceOrientation(SliceOrientation::Coronal);

    // 3: 3D (bottom-right)
    if (!impl_->viewports[3]) {
        impl_->viewports[3] = new ViewportWidget(impl_->quadContainer);
        impl_->viewports[3]->setMode(ViewportMode::VolumeRendering);
    }
    impl_->viewports[3]->setParent(impl_->quadContainer);

    grid->addWidget(impl_->viewports[0], 0, 0);
    grid->addWidget(impl_->viewports[1], 0, 1);
    grid->addWidget(impl_->viewports[2], 1, 0);
    grid->addWidget(impl_->viewports[3], 1, 1);

    // Equal stretch
    grid->setRowStretch(0, 1);
    grid->setRowStretch(1, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);

    impl_->stack->addWidget(impl_->quadContainer);
    impl_->layoutBuilt[2] = true;
}

} // namespace dicom_viewer::ui

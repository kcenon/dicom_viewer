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
    int activeIndex = 0;
    QStackedWidget* stack = nullptr;

    // Layout containers (one per mode)
    QWidget* singleContainer = nullptr;
    QSplitter* dualContainer = nullptr;
    QWidget* quadContainer = nullptr;

    // Viewports: max 4 (Axial, Sagittal, Coronal, 3D)
    // Index 0 is always the "primary" viewport.
    ViewportWidget* viewports[4] = {};
    bool layoutBuilt[3] = {};  // Track which layouts have been built

    // Crosshair linking
    bool crosshairLinkEnabled = false;
    bool propagatingCrosshair = false;  // Guard against feedback loops
    QList<QMetaObject::Connection> crosshairConnections;
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

int ViewportLayoutManager::activeViewportIndex() const
{
    return impl_->activeIndex;
}

ViewportWidget* ViewportLayoutManager::activeViewport() const
{
    return impl_->viewports[impl_->activeIndex];
}

void ViewportLayoutManager::setActiveViewport(int index)
{
    if (index < 0 || index >= viewportCount()) return;
    if (impl_->activeIndex == index) return;
    impl_->activeIndex = index;
    emit activeViewportChanged(impl_->viewports[index], index);
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

    // Reconnect crosshair linking for new layout
    if (impl_->crosshairLinkEnabled) {
        teardownCrosshairLinking();
        setupCrosshairLinking();
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
    // Outer vertical splitter: top row | bottom row
    auto* outerSplitter = new QSplitter(Qt::Vertical, impl_->stack);
    outerSplitter->setHandleWidth(3);

    // Top row: Axial | Sagittal
    auto* topSplitter = new QSplitter(Qt::Horizontal, outerSplitter);
    topSplitter->setHandleWidth(3);

    // Bottom row: Coronal | 3D
    auto* bottomSplitter = new QSplitter(Qt::Horizontal, outerSplitter);
    bottomSplitter->setHandleWidth(3);

    // 0: Axial (top-left)
    if (!impl_->viewports[0]) {
        impl_->viewports[0] = new ViewportWidget(topSplitter);
    }
    impl_->viewports[0]->setParent(topSplitter);
    impl_->viewports[0]->setSliceOrientation(SliceOrientation::Axial);
    topSplitter->addWidget(impl_->viewports[0]);

    // 1: Sagittal (top-right)
    if (!impl_->viewports[1]) {
        impl_->viewports[1] = new ViewportWidget(topSplitter);
    }
    impl_->viewports[1]->setParent(topSplitter);
    impl_->viewports[1]->setSliceOrientation(SliceOrientation::Sagittal);
    topSplitter->addWidget(impl_->viewports[1]);

    // 2: Coronal (bottom-left)
    if (!impl_->viewports[2]) {
        impl_->viewports[2] = new ViewportWidget(bottomSplitter);
    }
    impl_->viewports[2]->setParent(bottomSplitter);
    impl_->viewports[2]->setSliceOrientation(SliceOrientation::Coronal);
    bottomSplitter->addWidget(impl_->viewports[2]);

    // 3: 3D (bottom-right)
    if (!impl_->viewports[3]) {
        impl_->viewports[3] = new ViewportWidget(bottomSplitter);
        impl_->viewports[3]->setMode(ViewportMode::VolumeRendering);
    }
    impl_->viewports[3]->setParent(bottomSplitter);
    bottomSplitter->addWidget(impl_->viewports[3]);

    // Equal split for both rows and columns
    topSplitter->setSizes({1, 1});
    bottomSplitter->setSizes({1, 1});
    outerSplitter->addWidget(topSplitter);
    outerSplitter->addWidget(bottomSplitter);
    outerSplitter->setSizes({1, 1});

    impl_->quadContainer = outerSplitter;
    impl_->stack->addWidget(outerSplitter);
    impl_->layoutBuilt[2] = true;
}

bool ViewportLayoutManager::isCrosshairLinkEnabled() const
{
    return impl_->crosshairLinkEnabled;
}

void ViewportLayoutManager::setCrosshairLinkEnabled(bool enabled)
{
    if (impl_->crosshairLinkEnabled == enabled) return;
    impl_->crosshairLinkEnabled = enabled;

    if (enabled) {
        setupCrosshairLinking();
    } else {
        teardownCrosshairLinking();
    }

    emit crosshairLinkEnabledChanged(enabled);
}

void ViewportLayoutManager::setupCrosshairLinking()
{
    teardownCrosshairLinking();

    int count = viewportCount();
    for (int src = 0; src < count; ++src) {
        auto* srcVp = impl_->viewports[src];
        if (!srcVp) continue;

        // Enable crosshair lines on 2D viewports
        srcVp->setCrosshairLinesVisible(true);

        // Connect crosshair signal to all other viewports
        auto conn = connect(srcVp, &ViewportWidget::crosshairPositionChanged,
                            this, [this, src](double x, double y, double z) {
            if (impl_->propagatingCrosshair) return;
            impl_->propagatingCrosshair = true;

            int cnt = viewportCount();
            for (int dst = 0; dst < cnt; ++dst) {
                if (dst == src) continue;
                auto* dstVp = impl_->viewports[dst];
                if (dstVp) {
                    dstVp->setCrosshairPosition(x, y, z);
                }
            }

            impl_->propagatingCrosshair = false;
        });
        impl_->crosshairConnections.append(conn);
    }
}

void ViewportLayoutManager::teardownCrosshairLinking()
{
    for (auto& conn : impl_->crosshairConnections) {
        disconnect(conn);
    }
    impl_->crosshairConnections.clear();

    // Hide crosshair lines on all viewports
    for (int i = 0; i < 4; ++i) {
        if (impl_->viewports[i]) {
            impl_->viewports[i]->setCrosshairLinesVisible(false);
        }
    }
}

} // namespace dicom_viewer::ui

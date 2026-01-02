#include "services/segmentation/label_map_overlay.hpp"

#include <itkImageToVTKImageFilter.h>
#include <itkExtractImageFilter.h>

#include <vtkImageActor.h>
#include <vtkImageMapToColors.h>
#include <vtkLookupTable.h>
#include <vtkImageReslice.h>
#include <vtkMatrix4x4.h>

#include <unordered_map>

namespace dicom_viewer::services {

namespace {

// Use LabelColorPalette for consistent colors

} // namespace

struct PlaneOverlayData {
    vtkSmartPointer<vtkRenderer> renderer;
    vtkSmartPointer<vtkImageReslice> reslice;
    vtkSmartPointer<vtkImageMapToColors> colorMapper;
    vtkSmartPointer<vtkImageActor> actor;
    double currentSlicePosition = 0.0;
};

class LabelMapOverlay::Impl {
public:
    LabelMapType::Pointer labelMap;
    vtkSmartPointer<vtkImageData> vtkLabelMap;
    vtkSmartPointer<vtkLookupTable> lookupTable;

    std::unordered_map<uint8_t, LabelColor> labelColors;
    std::array<std::optional<PlaneOverlayData>, 3> planeData;

    double opacity = 0.5;
    bool visible = true;

    // Image metadata
    std::array<double, 3> spacing = {1.0, 1.0, 1.0};
    std::array<double, 3> origin = {0.0, 0.0, 0.0};
    std::array<int, 3> dimensions = {0, 0, 0};

    Impl() {
        // Initialize lookup table
        lookupTable = vtkSmartPointer<vtkLookupTable>::New();
        buildLookupTable();
    }

    void buildLookupTable() {
        lookupTable->SetNumberOfTableValues(256);
        lookupTable->SetRange(0, 255);

        // Background (label 0) is transparent
        lookupTable->SetTableValue(0, 0.0, 0.0, 0.0, 0.0);

        // Set colors for labels 1-255
        for (int i = 1; i < 256; ++i) {
            LabelColor color;
            auto it = labelColors.find(static_cast<uint8_t>(i));
            if (it != labelColors.end()) {
                color = it->second;
            } else {
                // Use palette colors (float-based)
                color = LabelColorPalette::getColor(static_cast<uint8_t>(i));
            }

            // LabelColor uses float [0.0, 1.0] values
            lookupTable->SetTableValue(
                i,
                color.r,
                color.g,
                color.b,
                color.a * opacity);
        }

        lookupTable->Build();
    }

    void updateVTKLabelMap() {
        if (!labelMap) {
            vtkLabelMap = nullptr;
            return;
        }

        // Convert ITK to VTK image
        using FilterType = itk::ImageToVTKImageFilter<LabelMapType>;
        auto filter = FilterType::New();
        filter->SetInput(labelMap);
        filter->Update();

        vtkLabelMap = vtkSmartPointer<vtkImageData>::New();
        vtkLabelMap->DeepCopy(filter->GetOutput());

        // Update metadata
        labelMap->GetSpacing(spacing.data());
        auto itkOrigin = labelMap->GetOrigin();
        origin = {itkOrigin[0], itkOrigin[1], itkOrigin[2]};

        auto region = labelMap->GetLargestPossibleRegion();
        auto size = region.GetSize();
        dimensions = {
            static_cast<int>(size[0]),
            static_cast<int>(size[1]),
            static_cast<int>(size[2])
        };
    }

    void setupPlaneOverlay(MPRPlane plane) {
        int index = static_cast<int>(plane);
        if (!planeData[index].has_value()) {
            return;
        }

        auto& data = *planeData[index];
        if (!vtkLabelMap || !data.renderer) {
            return;
        }

        // Setup reslice
        data.reslice = vtkSmartPointer<vtkImageReslice>::New();
        data.reslice->SetInputData(vtkLabelMap);
        data.reslice->SetOutputDimensionality(2);
        data.reslice->SetInterpolationModeToNearestNeighbor();  // Important for labels!

        // Setup reslice axes based on plane
        auto matrix = vtkSmartPointer<vtkMatrix4x4>::New();
        matrix->Identity();

        switch (plane) {
            case MPRPlane::Axial:
                // Default orientation (XY plane)
                break;
            case MPRPlane::Coronal:
                // Rotate -90 degrees around X axis
                matrix->SetElement(1, 1, 0);
                matrix->SetElement(1, 2, 1);
                matrix->SetElement(2, 1, -1);
                matrix->SetElement(2, 2, 0);
                break;
            case MPRPlane::Sagittal:
                // Rotate 90 degrees around Y axis
                matrix->SetElement(0, 0, 0);
                matrix->SetElement(0, 2, -1);
                matrix->SetElement(2, 0, 1);
                matrix->SetElement(2, 2, 0);
                break;
        }

        data.reslice->SetResliceAxes(matrix);

        // Setup color mapper
        data.colorMapper = vtkSmartPointer<vtkImageMapToColors>::New();
        data.colorMapper->SetLookupTable(lookupTable);
        data.colorMapper->SetInputConnection(data.reslice->GetOutputPort());
        data.colorMapper->SetOutputFormatToRGBA();

        // Setup actor
        data.actor = vtkSmartPointer<vtkImageActor>::New();
        data.actor->GetMapper()->SetInputConnection(data.colorMapper->GetOutputPort());
        data.actor->SetVisibility(visible);

        data.renderer->AddActor(data.actor);
    }

    void updatePlaneSlice(MPRPlane plane) {
        int index = static_cast<int>(plane);
        if (!planeData[index].has_value() || !vtkLabelMap) {
            return;
        }

        auto& data = *planeData[index];
        if (!data.reslice) {
            return;
        }

        auto matrix = data.reslice->GetResliceAxes();
        double position = data.currentSlicePosition;

        // Update the translation component based on plane
        switch (plane) {
            case MPRPlane::Axial:
                matrix->SetElement(2, 3, position);
                break;
            case MPRPlane::Coronal:
                matrix->SetElement(1, 3, position);
                break;
            case MPRPlane::Sagittal:
                matrix->SetElement(0, 3, position);
                break;
        }

        data.reslice->Modified();
        data.colorMapper->Update();
    }
};

LabelMapOverlay::LabelMapOverlay()
    : impl_(std::make_unique<Impl>()) {}

LabelMapOverlay::~LabelMapOverlay() = default;

LabelMapOverlay::LabelMapOverlay(LabelMapOverlay&&) noexcept = default;
LabelMapOverlay& LabelMapOverlay::operator=(LabelMapOverlay&&) noexcept = default;

void LabelMapOverlay::setLabelMap(LabelMapType::Pointer labelMap) {
    impl_->labelMap = labelMap;
    impl_->updateVTKLabelMap();

    // Re-setup all attached planes
    for (int i = 0; i < 3; ++i) {
        if (impl_->planeData[i].has_value()) {
            impl_->setupPlaneOverlay(static_cast<MPRPlane>(i));
            impl_->updatePlaneSlice(static_cast<MPRPlane>(i));
        }
    }
}

LabelMapOverlay::LabelMapType::Pointer LabelMapOverlay::getLabelMap() const {
    return impl_->labelMap;
}

void LabelMapOverlay::setLabelColor(uint8_t labelId, const LabelColor& color) {
    impl_->labelColors[labelId] = color;
    impl_->buildLookupTable();
}

LabelColor LabelMapOverlay::getLabelColor(uint8_t labelId) const {
    auto it = impl_->labelColors.find(labelId);
    if (it != impl_->labelColors.end()) {
        return it->second;
    }
    return LabelColorPalette::getColor(labelId);
}

void LabelMapOverlay::setOpacity(double opacity) {
    impl_->opacity = std::clamp(opacity, 0.0, 1.0);
    impl_->buildLookupTable();
}

double LabelMapOverlay::getOpacity() const {
    return impl_->opacity;
}

void LabelMapOverlay::setVisible(bool visible) {
    impl_->visible = visible;

    for (int i = 0; i < 3; ++i) {
        if (impl_->planeData[i].has_value() && impl_->planeData[i]->actor) {
            impl_->planeData[i]->actor->SetVisibility(visible);
        }
    }
}

bool LabelMapOverlay::isVisible() const {
    return impl_->visible;
}

void LabelMapOverlay::attachToRenderer(vtkSmartPointer<vtkRenderer> renderer,
                                        MPRPlane plane) {
    int index = static_cast<int>(plane);

    PlaneOverlayData data;
    data.renderer = renderer;
    impl_->planeData[index] = data;

    if (impl_->vtkLabelMap) {
        impl_->setupPlaneOverlay(plane);
    }
}

void LabelMapOverlay::detachFromRenderer(MPRPlane plane) {
    int index = static_cast<int>(plane);

    if (impl_->planeData[index].has_value()) {
        auto& data = *impl_->planeData[index];
        if (data.renderer && data.actor) {
            data.renderer->RemoveActor(data.actor);
        }
        impl_->planeData[index] = std::nullopt;
    }
}

void LabelMapOverlay::updateSlice(MPRPlane plane, double slicePosition) {
    int index = static_cast<int>(plane);

    if (!impl_->planeData[index].has_value()) {
        return;
    }

    impl_->planeData[index]->currentSlicePosition = slicePosition;
    impl_->updatePlaneSlice(plane);
}

void LabelMapOverlay::updateAll() {
    impl_->updateVTKLabelMap();
    impl_->buildLookupTable();

    for (int i = 0; i < 3; ++i) {
        if (impl_->planeData[i].has_value()) {
            impl_->updatePlaneSlice(static_cast<MPRPlane>(i));
        }
    }
}

void LabelMapOverlay::notifySliceModified(int sliceIndex) {
    // For now, update all views when any slice is modified
    // Future optimization: only update affected planes
    (void)sliceIndex;
    updateAll();
}

} // namespace dicom_viewer::services

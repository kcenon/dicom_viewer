#include "services/segmentation/mpr_segmentation_renderer.hpp"

#include <vtkImageActor.h>
#include <vtkImageData.h>
#include <vtkImageMapToColors.h>
#include <vtkImageSlice.h>
#include <vtkImageSliceMapper.h>
#include <vtkLookupTable.h>
#include <vtkImageReslice.h>
#include <vtkMatrix4x4.h>
#include <vtkImageProperty.h>

#include <itkImageToVTKImageFilter.h>
#include <itkExtractImageFilter.h>

#include <algorithm>
#include <unordered_map>

namespace dicom_viewer::services {

class MPRSegmentationRenderer::Impl {
public:
    LabelMapType::Pointer labelMap;
    LabelManager* labelManager = nullptr;

    // Renderers for each plane
    std::array<vtkSmartPointer<vtkRenderer>, 3> renderers;

    // VTK pipeline components for each plane
    std::array<vtkSmartPointer<vtkImageData>, 3> sliceData;
    std::array<vtkSmartPointer<vtkImageMapToColors>, 3> colorMappers;
    std::array<vtkSmartPointer<vtkImageActor>, 3> imageActors;
    std::array<vtkSmartPointer<vtkLookupTable>, 3> lookupTables;

    // Current slice indices for each plane
    std::array<int, 3> sliceIndices = {0, 0, 0};

    // Visibility settings
    bool visible = true;
    double opacity = 0.5;
    std::unordered_map<uint8_t, bool> labelVisibility;
    std::unordered_map<uint8_t, LabelColor> labelColors;

    // Callback
    UpdateCallback updateCallback;

    Impl() {
        // Initialize VTK components for each plane
        for (int i = 0; i < 3; ++i) {
            sliceData[i] = vtkSmartPointer<vtkImageData>::New();
            lookupTables[i] = vtkSmartPointer<vtkLookupTable>::New();
            colorMappers[i] = vtkSmartPointer<vtkImageMapToColors>::New();
            imageActors[i] = vtkSmartPointer<vtkImageActor>::New();

            // Setup lookup table for label colors
            setupDefaultLookupTable(lookupTables[i]);

            // Setup color mapper
            colorMappers[i]->SetLookupTable(lookupTables[i]);
            colorMappers[i]->SetOutputFormatToRGBA();

            // Setup image actor
            imageActors[i]->GetMapper()->SetInputConnection(colorMappers[i]->GetOutputPort());
            imageActors[i]->SetOpacity(opacity);

            // Make sure overlay is rendered on top
            auto property = imageActors[i]->GetProperty();
            property->SetInterpolationTypeToNearest();
        }
    }

    void setupDefaultLookupTable(vtkSmartPointer<vtkLookupTable> lut) {
        // 256 entries for label IDs 0-255
        lut->SetNumberOfTableValues(256);
        lut->SetRange(0, 255);

        // Label 0 is transparent (background)
        lut->SetTableValue(0, 0.0, 0.0, 0.0, 0.0);

        // Default colors for labels 1-255
        // Use a color palette that provides good contrast
        for (int i = 1; i < 256; ++i) {
            double hue = (i * 0.618033988749895) - std::floor(i * 0.618033988749895);
            double sat = 0.8;
            double val = 0.9;

            // HSV to RGB conversion
            double c = val * sat;
            double x = c * (1 - std::abs(std::fmod(hue * 6, 2) - 1));
            double m = val - c;

            double r, g, b;
            if (hue < 1.0/6.0) { r = c; g = x; b = 0; }
            else if (hue < 2.0/6.0) { r = x; g = c; b = 0; }
            else if (hue < 3.0/6.0) { r = 0; g = c; b = x; }
            else if (hue < 4.0/6.0) { r = 0; g = x; b = c; }
            else if (hue < 5.0/6.0) { r = x; g = 0; b = c; }
            else { r = c; g = 0; b = x; }

            lut->SetTableValue(i, r + m, g + m, b + m, 1.0);
        }

        lut->Build();
    }

    void updateLookupTable(int planeIndex) {
        auto lut = lookupTables[planeIndex];

        // Update from label manager if available
        if (labelManager) {
            for (const auto& label : labelManager->getAllLabels()) {
                bool isVisible = labelVisibility.count(label.id) ?
                                 labelVisibility[label.id] : label.visible;

                if (isVisible) {
                    lut->SetTableValue(label.id,
                        label.color.r, label.color.g, label.color.b, label.color.a);
                } else {
                    lut->SetTableValue(label.id, 0.0, 0.0, 0.0, 0.0);
                }
            }
        }

        // Apply manual overrides
        for (const auto& [labelId, color] : labelColors) {
            bool isVisible = labelVisibility.count(labelId) ?
                             labelVisibility[labelId] : true;

            if (isVisible) {
                lut->SetTableValue(labelId, color.r, color.g, color.b, color.a);
            } else {
                lut->SetTableValue(labelId, 0.0, 0.0, 0.0, 0.0);
            }
        }

        lut->Modified();
    }

    void extractSlice(MPRPlane plane) {
        if (!labelMap) {
            return;
        }

        int planeIndex = static_cast<int>(plane);
        int sliceIdx = sliceIndices[planeIndex];

        auto region = labelMap->GetLargestPossibleRegion();
        auto size = region.GetSize();
        auto index = region.GetIndex();

        // Create extraction region based on plane
        LabelMapType::RegionType extractRegion;
        LabelMapType::SizeType extractSize;
        LabelMapType::IndexType extractIndex;

        switch (plane) {
            case MPRPlane::Axial:
                // Extract XY plane at Z = sliceIdx
                extractSize[0] = size[0];
                extractSize[1] = size[1];
                extractSize[2] = 0;  // Collapse Z dimension
                extractIndex[0] = index[0];
                extractIndex[1] = index[1];
                extractIndex[2] = std::clamp(sliceIdx, 0, static_cast<int>(size[2]) - 1);
                break;

            case MPRPlane::Coronal:
                // Extract XZ plane at Y = sliceIdx
                extractSize[0] = size[0];
                extractSize[1] = 0;  // Collapse Y dimension
                extractSize[2] = size[2];
                extractIndex[0] = index[0];
                extractIndex[1] = std::clamp(sliceIdx, 0, static_cast<int>(size[1]) - 1);
                extractIndex[2] = index[2];
                break;

            case MPRPlane::Sagittal:
                // Extract YZ plane at X = sliceIdx
                extractSize[0] = 0;  // Collapse X dimension
                extractSize[1] = size[1];
                extractSize[2] = size[2];
                extractIndex[0] = std::clamp(sliceIdx, 0, static_cast<int>(size[0]) - 1);
                extractIndex[1] = index[1];
                extractIndex[2] = index[2];
                break;
        }

        extractRegion.SetSize(extractSize);
        extractRegion.SetIndex(extractIndex);

        // Extract 2D slice
        using ExtractFilterType = itk::ExtractImageFilter<LabelMapType, itk::Image<uint8_t, 2>>;
        auto extractFilter = ExtractFilterType::New();
        extractFilter->SetInput(labelMap);
        extractFilter->SetExtractionRegion(extractRegion);
        extractFilter->SetDirectionCollapseToSubmatrix();
        extractFilter->Update();

        // Convert to VTK image
        using ConverterType = itk::ImageToVTKImageFilter<itk::Image<uint8_t, 2>>;
        auto converter = ConverterType::New();
        converter->SetInput(extractFilter->GetOutput());
        converter->Update();

        // Store the slice data
        sliceData[planeIndex]->DeepCopy(converter->GetOutput());
        colorMappers[planeIndex]->SetInputData(sliceData[planeIndex]);
        colorMappers[planeIndex]->Modified();
    }

    void addActorsToRenderers() {
        for (int i = 0; i < 3; ++i) {
            if (renderers[i]) {
                // Check if actor is already added
                if (!renderers[i]->HasViewProp(imageActors[i])) {
                    renderers[i]->AddActor(imageActors[i]);
                }
            }
        }
    }

    void removeActorsFromRenderers() {
        for (int i = 0; i < 3; ++i) {
            if (renderers[i]) {
                renderers[i]->RemoveActor(imageActors[i]);
            }
        }
    }

    void updateVisibility() {
        for (int i = 0; i < 3; ++i) {
            imageActors[i]->SetVisibility(visible && labelMap != nullptr);
            imageActors[i]->SetOpacity(opacity);
        }
    }
};

MPRSegmentationRenderer::MPRSegmentationRenderer()
    : impl_(std::make_unique<Impl>()) {}

MPRSegmentationRenderer::~MPRSegmentationRenderer() {
    if (impl_) {
        removeFromRenderers();
    }
}

MPRSegmentationRenderer::MPRSegmentationRenderer(MPRSegmentationRenderer&&) noexcept = default;
MPRSegmentationRenderer& MPRSegmentationRenderer::operator=(MPRSegmentationRenderer&&) noexcept = default;

void MPRSegmentationRenderer::setLabelMap(LabelMapType::Pointer labelMap) {
    impl_->labelMap = labelMap;

    if (labelMap) {
        // Extract initial slices for all planes
        for (int i = 0; i < 3; ++i) {
            impl_->extractSlice(static_cast<MPRPlane>(i));
        }
        impl_->addActorsToRenderers();
    }

    impl_->updateVisibility();

    if (impl_->updateCallback) {
        impl_->updateCallback();
    }
}

MPRSegmentationRenderer::LabelMapType::Pointer
MPRSegmentationRenderer::getLabelMap() const {
    return impl_->labelMap;
}

void MPRSegmentationRenderer::setRenderers(
    vtkSmartPointer<vtkRenderer> axialRenderer,
    vtkSmartPointer<vtkRenderer> coronalRenderer,
    vtkSmartPointer<vtkRenderer> sagittalRenderer) {

    impl_->removeActorsFromRenderers();

    impl_->renderers[0] = axialRenderer;
    impl_->renderers[1] = coronalRenderer;
    impl_->renderers[2] = sagittalRenderer;

    impl_->addActorsToRenderers();
}

void MPRSegmentationRenderer::setRenderer(MPRPlane plane, vtkSmartPointer<vtkRenderer> renderer) {
    int idx = static_cast<int>(plane);

    if (impl_->renderers[idx]) {
        impl_->renderers[idx]->RemoveActor(impl_->imageActors[idx]);
    }

    impl_->renderers[idx] = renderer;

    if (renderer && impl_->labelMap) {
        renderer->AddActor(impl_->imageActors[idx]);
    }
}

void MPRSegmentationRenderer::setLabelManager(LabelManager* labelManager) {
    impl_->labelManager = labelManager;

    // Update all lookup tables with label manager colors
    for (int i = 0; i < 3; ++i) {
        impl_->updateLookupTable(i);
    }
}

void MPRSegmentationRenderer::setSliceIndex(MPRPlane plane, int sliceIndex) {
    int idx = static_cast<int>(plane);
    impl_->sliceIndices[idx] = sliceIndex;
    impl_->extractSlice(plane);

    if (impl_->updateCallback) {
        impl_->updateCallback();
    }
}

int MPRSegmentationRenderer::getSliceIndex(MPRPlane plane) const {
    return impl_->sliceIndices[static_cast<int>(plane)];
}

void MPRSegmentationRenderer::setVisible(bool visible) {
    impl_->visible = visible;
    impl_->updateVisibility();

    if (impl_->updateCallback) {
        impl_->updateCallback();
    }
}

bool MPRSegmentationRenderer::isVisible() const {
    return impl_->visible;
}

void MPRSegmentationRenderer::setLabelVisible(uint8_t labelId, bool visible) {
    impl_->labelVisibility[labelId] = visible;

    for (int i = 0; i < 3; ++i) {
        impl_->updateLookupTable(i);
    }

    if (impl_->updateCallback) {
        impl_->updateCallback();
    }
}

void MPRSegmentationRenderer::setLabelColor(uint8_t labelId, const LabelColor& color) {
    impl_->labelColors[labelId] = color;

    for (int i = 0; i < 3; ++i) {
        impl_->updateLookupTable(i);
    }

    if (impl_->updateCallback) {
        impl_->updateCallback();
    }
}

void MPRSegmentationRenderer::setOpacity(double opacity) {
    impl_->opacity = std::clamp(opacity, 0.0, 1.0);
    impl_->updateVisibility();

    if (impl_->updateCallback) {
        impl_->updateCallback();
    }
}

double MPRSegmentationRenderer::getOpacity() const {
    return impl_->opacity;
}

void MPRSegmentationRenderer::update() {
    if (!impl_->labelMap) {
        return;
    }

    for (int i = 0; i < 3; ++i) {
        impl_->extractSlice(static_cast<MPRPlane>(i));
        impl_->updateLookupTable(i);
    }

    if (impl_->updateCallback) {
        impl_->updateCallback();
    }
}

void MPRSegmentationRenderer::updatePlane(MPRPlane plane) {
    if (!impl_->labelMap) {
        return;
    }

    impl_->extractSlice(plane);
    impl_->updateLookupTable(static_cast<int>(plane));

    if (impl_->updateCallback) {
        impl_->updateCallback();
    }
}

void MPRSegmentationRenderer::setUpdateCallback(UpdateCallback callback) {
    impl_->updateCallback = std::move(callback);
}

void MPRSegmentationRenderer::removeFromRenderers() {
    impl_->removeActorsFromRenderers();
}

void MPRSegmentationRenderer::clear() {
    impl_->labelMap = nullptr;
    impl_->updateVisibility();

    if (impl_->updateCallback) {
        impl_->updateCallback();
    }
}

} // namespace dicom_viewer::services

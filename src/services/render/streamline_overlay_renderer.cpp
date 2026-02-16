#include "services/render/streamline_overlay_renderer.hpp"
#include "services/render/hemodynamic_overlay_renderer.hpp"
#include "services/mpr_renderer.hpp"

#include <vtkActor.h>
#include <vtkFloatArray.h>
#include <vtkImageActor.h>
#include <vtkImageData.h>
#include <vtkImageMapper3D.h>
#include <vtkImageMapToColors.h>
#include <vtkLookupTable.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRungeKutta45.h>
#include <vtkStreamTracer.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace dicom_viewer::services {

// =============================================================================
// Implementation
// =============================================================================

class StreamlineOverlayRenderer::Impl {
public:
    // Input data
    vtkSmartPointer<vtkImageData> velocityField;

    // Settings
    Mode mode = Mode::Streamline;
    bool visible = true;
    double overlayOpacity = 0.6;
    Streamline2DParams streamlineParams;
    LICParams licParams;

    // Per-plane rendering state
    std::array<vtkSmartPointer<vtkRenderer>, 3> renderers;
    std::array<double, 3> slicePositions = {0.0, 0.0, 0.0};

    // Streamline mode: polydata actors
    std::array<vtkSmartPointer<vtkActor>, 3> streamlineActors;

    // LIC mode: image actors
    std::array<vtkSmartPointer<vtkImageActor>, 3> licActors;

    Impl() {
        for (int i = 0; i < 3; ++i) {
            streamlineActors[i] = vtkSmartPointer<vtkActor>::New();
            streamlineActors[i]->SetVisibility(false);
            streamlineActors[i]->GetProperty()->SetOpacity(overlayOpacity);

            licActors[i] = vtkSmartPointer<vtkImageActor>::New();
            licActors[i]->SetVisibility(false);
            licActors[i]->SetOpacity(overlayOpacity);
        }
    }

    void applyVisibility() {
        bool hasData = velocityField != nullptr;
        for (int i = 0; i < 3; ++i) {
            if (mode == Mode::Streamline) {
                streamlineActors[i]->SetVisibility(visible && hasData);
                licActors[i]->SetVisibility(false);
            } else {
                streamlineActors[i]->SetVisibility(false);
                licActors[i]->SetVisibility(visible && hasData);
            }
        }
    }

    void attachToRenderers() {
        for (int i = 0; i < 3; ++i) {
            if (renderers[i]) {
                renderers[i]->AddViewProp(streamlineActors[i]);
                renderers[i]->AddViewProp(licActors[i]);
            }
        }
    }

    void detachFromRenderers() {
        for (int i = 0; i < 3; ++i) {
            if (renderers[i]) {
                renderers[i]->RemoveViewProp(streamlineActors[i]);
                renderers[i]->RemoveViewProp(licActors[i]);
            }
        }
    }

    void regeneratePlane(int planeIndex) {
        if (!velocityField) {
            return;
        }

        auto mprPlane = static_cast<MPRPlane>(planeIndex);
        double position = slicePositions[planeIndex];

        auto sliceResult = StreamlineOverlayRenderer::extractSliceVelocity(
            velocityField, mprPlane, position);
        if (!sliceResult.has_value()) {
            return;
        }

        auto& velocitySlice = *sliceResult;

        if (mode == Mode::Streamline) {
            auto polyResult = StreamlineOverlayRenderer::generateStreamlines2D(
                velocitySlice, streamlineParams);
            if (polyResult.has_value()) {
                auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
                mapper->SetInputData(*polyResult);
                mapper->SetScalarModeToUsePointData();
                mapper->SetColorModeToMapScalars();

                // Build a Jet LUT for velocity magnitude coloring
                auto lut = vtkSmartPointer<vtkLookupTable>::New();
                lut->SetNumberOfTableValues(256);
                lut->SetHueRange(0.667, 0.0);
                lut->SetSaturationRange(1.0, 1.0);
                lut->SetValueRange(1.0, 1.0);
                lut->Build();

                double range[2];
                if ((*polyResult)->GetPointData()->GetScalars()) {
                    (*polyResult)->GetPointData()->GetScalars()->GetRange(range);
                    lut->SetTableRange(range);
                }
                mapper->SetLookupTable(lut);
                mapper->ScalarVisibilityOn();

                streamlineActors[planeIndex]->SetMapper(mapper);
                streamlineActors[planeIndex]->GetProperty()->SetLineWidth(
                    static_cast<float>(streamlineParams.lineWidth));
            }
        } else {
            auto licResult = StreamlineOverlayRenderer::computeLIC(
                velocitySlice, licParams);
            if (licResult.has_value()) {
                licActors[planeIndex]->GetMapper()->SetInputData(*licResult);
            }
        }

        applyVisibility();
    }
};

// =============================================================================
// Public API
// =============================================================================

StreamlineOverlayRenderer::StreamlineOverlayRenderer()
    : impl_(std::make_unique<Impl>()) {}

StreamlineOverlayRenderer::~StreamlineOverlayRenderer() {
    if (impl_) {
        impl_->detachFromRenderers();
    }
}

StreamlineOverlayRenderer::StreamlineOverlayRenderer(StreamlineOverlayRenderer&&) noexcept = default;
StreamlineOverlayRenderer& StreamlineOverlayRenderer::operator=(StreamlineOverlayRenderer&&) noexcept = default;

void StreamlineOverlayRenderer::setVelocityField(vtkSmartPointer<vtkImageData> velocityField) {
    impl_->velocityField = velocityField;
    impl_->applyVisibility();
}

bool StreamlineOverlayRenderer::hasVelocityField() const noexcept {
    return impl_->velocityField != nullptr;
}

void StreamlineOverlayRenderer::setMode(Mode mode) {
    impl_->mode = mode;
    impl_->applyVisibility();
}

StreamlineOverlayRenderer::Mode StreamlineOverlayRenderer::mode() const noexcept {
    return impl_->mode;
}

void StreamlineOverlayRenderer::setVisible(bool visible) {
    impl_->visible = visible;
    impl_->applyVisibility();
}

bool StreamlineOverlayRenderer::isVisible() const noexcept {
    return impl_->visible;
}

void StreamlineOverlayRenderer::setOpacity(double opacity) {
    impl_->overlayOpacity = std::clamp(opacity, 0.0, 1.0);
    for (int i = 0; i < 3; ++i) {
        impl_->streamlineActors[i]->GetProperty()->SetOpacity(impl_->overlayOpacity);
        impl_->licActors[i]->SetOpacity(impl_->overlayOpacity);
    }
}

double StreamlineOverlayRenderer::opacity() const noexcept {
    return impl_->overlayOpacity;
}

void StreamlineOverlayRenderer::setStreamlineParams(const Streamline2DParams& params) {
    impl_->streamlineParams = params;
}

void StreamlineOverlayRenderer::setLICParams(const LICParams& params) {
    impl_->licParams = params;
}

void StreamlineOverlayRenderer::setRenderers(
    vtkSmartPointer<vtkRenderer> axial,
    vtkSmartPointer<vtkRenderer> coronal,
    vtkSmartPointer<vtkRenderer> sagittal) {

    impl_->detachFromRenderers();

    impl_->renderers[0] = axial;
    impl_->renderers[1] = coronal;
    impl_->renderers[2] = sagittal;

    impl_->attachToRenderers();
    impl_->applyVisibility();
}

std::expected<void, OverlayError>
StreamlineOverlayRenderer::setSlicePosition(MPRPlane plane, double worldPosition) {
    if (!impl_->velocityField) {
        return std::unexpected(OverlayError::NoScalarField);
    }

    int planeIndex = static_cast<int>(plane);
    if (planeIndex < 0 || planeIndex > 2) {
        return std::unexpected(OverlayError::InvalidPlane);
    }

    impl_->slicePositions[planeIndex] = worldPosition;
    return {};
}

void StreamlineOverlayRenderer::update() {
    for (int i = 0; i < 3; ++i) {
        impl_->regeneratePlane(i);
    }
}

void StreamlineOverlayRenderer::updatePlane(MPRPlane plane) {
    int idx = static_cast<int>(plane);
    if (idx >= 0 && idx <= 2) {
        impl_->regeneratePlane(idx);
    }
}

// =============================================================================
// Static: Extract 2D slice velocity
// =============================================================================

std::expected<vtkSmartPointer<vtkImageData>, OverlayError>
StreamlineOverlayRenderer::extractSliceVelocity(
    vtkSmartPointer<vtkImageData> velocityField,
    MPRPlane plane, double worldPosition) {

    if (!velocityField) {
        return std::unexpected(OverlayError::NoScalarField);
    }

    if (velocityField->GetNumberOfScalarComponents() < 3) {
        return std::unexpected(OverlayError::NoScalarField);
    }

    int* dims = velocityField->GetDimensions();
    double* spacing = velocityField->GetSpacing();
    double* origin = velocityField->GetOrigin();

    // Determine which axis to slice and extract in-plane velocity components
    // Axial (XY plane): slice along Z, extract (Vx, Vy)
    // Coronal (XZ plane): slice along Y, extract (Vx, Vz)
    // Sagittal (YZ plane): slice along X, extract (Vy, Vz)

    int sliceAxis = -1;
    int comp0 = -1, comp1 = -1;
    int outDimX = 0, outDimY = 0;
    double outSpacingX = 0.0, outSpacingY = 0.0;
    double outOriginX = 0.0, outOriginY = 0.0;

    switch (plane) {
        case MPRPlane::Axial:
            sliceAxis = 2;  // Z
            comp0 = 0; comp1 = 1;  // Vx, Vy
            outDimX = dims[0]; outDimY = dims[1];
            outSpacingX = spacing[0]; outSpacingY = spacing[1];
            outOriginX = origin[0]; outOriginY = origin[1];
            break;
        case MPRPlane::Coronal:
            sliceAxis = 1;  // Y
            comp0 = 0; comp1 = 2;  // Vx, Vz
            outDimX = dims[0]; outDimY = dims[2];
            outSpacingX = spacing[0]; outSpacingY = spacing[2];
            outOriginX = origin[0]; outOriginY = origin[2];
            break;
        case MPRPlane::Sagittal:
            sliceAxis = 0;  // X
            comp0 = 1; comp1 = 2;  // Vy, Vz
            outDimX = dims[1]; outDimY = dims[2];
            outSpacingX = spacing[1]; outSpacingY = spacing[2];
            outOriginX = origin[1]; outOriginY = origin[2];
            break;
    }

    // Find the slice index closest to worldPosition
    int sliceDim = dims[sliceAxis];
    double sliceOrigin = origin[sliceAxis];
    double sliceSpacing = spacing[sliceAxis];

    int sliceIndex = static_cast<int>(
        std::round((worldPosition - sliceOrigin) / sliceSpacing));
    sliceIndex = std::clamp(sliceIndex, 0, sliceDim - 1);

    // Create 2D output with 3-component vectors (in-plane velocity + 0 for z)
    auto output = vtkSmartPointer<vtkImageData>::New();
    output->SetDimensions(outDimX, outDimY, 1);
    output->SetSpacing(outSpacingX, outSpacingY, 1.0);
    output->SetOrigin(outOriginX, outOriginY, 0.0);
    output->AllocateScalars(VTK_FLOAT, 3);

    // Also create a velocity magnitude array for scalar coloring
    auto magnitudeArray = vtkSmartPointer<vtkFloatArray>::New();
    magnitudeArray->SetName("VelocityMagnitude");
    magnitudeArray->SetNumberOfTuples(
        static_cast<vtkIdType>(outDimX) * outDimY);

    auto* inData = velocityField->GetPointData()->GetScalars();
    auto* outPtr = static_cast<float*>(output->GetScalarPointer());

    vtkIdType outIdx = 0;
    for (int j = 0; j < outDimY; ++j) {
        for (int i = 0; i < outDimX; ++i) {
            // Map (i, j) back to 3D indices depending on plane
            vtkIdType inIdx = 0;
            switch (plane) {
                case MPRPlane::Axial:
                    inIdx = static_cast<vtkIdType>(sliceIndex) * dims[1] * dims[0]
                          + static_cast<vtkIdType>(j) * dims[0] + i;
                    break;
                case MPRPlane::Coronal:
                    inIdx = static_cast<vtkIdType>(j) * dims[1] * dims[0]
                          + static_cast<vtkIdType>(sliceIndex) * dims[0] + i;
                    break;
                case MPRPlane::Sagittal:
                    inIdx = static_cast<vtkIdType>(j) * dims[1] * dims[0]
                          + static_cast<vtkIdType>(i) * dims[0] + sliceIndex;
                    break;
            }

            double v0 = inData->GetComponent(inIdx, comp0);
            double v1 = inData->GetComponent(inIdx, comp1);

            outPtr[outIdx * 3 + 0] = static_cast<float>(v0);
            outPtr[outIdx * 3 + 1] = static_cast<float>(v1);
            outPtr[outIdx * 3 + 2] = 0.0f;

            double mag = std::sqrt(v0 * v0 + v1 * v1);
            magnitudeArray->SetValue(outIdx, static_cast<float>(mag));

            ++outIdx;
        }
    }

    // Set vectors for vtkStreamTracer (needs active vectors)
    auto vectors = vtkSmartPointer<vtkFloatArray>::New();
    vectors->SetName("Velocity");
    vectors->SetNumberOfComponents(3);
    vectors->SetNumberOfTuples(static_cast<vtkIdType>(outDimX) * outDimY);

    outPtr = static_cast<float*>(output->GetScalarPointer());
    for (vtkIdType k = 0; k < static_cast<vtkIdType>(outDimX) * outDimY; ++k) {
        vectors->SetTuple3(k, outPtr[k * 3], outPtr[k * 3 + 1], outPtr[k * 3 + 2]);
    }

    output->GetPointData()->SetVectors(vectors);
    output->GetPointData()->AddArray(magnitudeArray);

    return output;
}

// =============================================================================
// Static: Generate 2D streamlines
// =============================================================================

std::expected<vtkSmartPointer<vtkPolyData>, OverlayError>
StreamlineOverlayRenderer::generateStreamlines2D(
    vtkSmartPointer<vtkImageData> velocitySlice,
    const Streamline2DParams& params) {

    if (!velocitySlice) {
        return std::unexpected(OverlayError::NoScalarField);
    }

    if (!velocitySlice->GetPointData()->GetVectors()) {
        return std::unexpected(OverlayError::NoScalarField);
    }

    int* dims = velocitySlice->GetDimensions();
    double* spacing = velocitySlice->GetSpacing();
    double* origin = velocitySlice->GetOrigin();

    // Create uniform grid seed points across the 2D slice
    auto seedPoints = vtkSmartPointer<vtkPoints>::New();
    int nx = static_cast<int>(std::sqrt(static_cast<double>(params.numSeedPoints)));
    int ny = (nx > 0) ? params.numSeedPoints / nx : 1;
    nx = std::max(nx, 1);
    ny = std::max(ny, 1);

    double dx = (dims[0] > 1) ? (dims[0] - 1) * spacing[0] / static_cast<double>(nx) : spacing[0];
    double dy = (dims[1] > 1) ? (dims[1] - 1) * spacing[1] / static_cast<double>(ny) : spacing[1];

    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            double x = origin[0] + (i + 0.5) * dx;
            double y = origin[1] + (j + 0.5) * dy;
            seedPoints->InsertNextPoint(x, y, 0.0);
        }
    }

    auto seedPolyData = vtkSmartPointer<vtkPolyData>::New();
    seedPolyData->SetPoints(seedPoints);

    // Run stream tracer
    auto tracer = vtkSmartPointer<vtkStreamTracer>::New();
    tracer->SetInputData(velocitySlice);
    tracer->SetSourceData(seedPolyData);

    auto integrator = vtkSmartPointer<vtkRungeKutta45>::New();
    tracer->SetIntegrator(integrator);

    tracer->SetMaximumPropagation(params.maxSteps * params.stepLength);
    tracer->SetInitialIntegrationStep(params.stepLength);
    tracer->SetIntegrationDirectionToBoth();
    tracer->SetTerminalSpeed(params.terminalSpeed);
    tracer->SetMaximumNumberOfSteps(params.maxSteps);

    tracer->Update();

    auto output = vtkSmartPointer<vtkPolyData>::New();
    output->DeepCopy(tracer->GetOutput());

    // Add velocity magnitude as scalars for color-coding
    if (output->GetNumberOfPoints() > 0) {
        auto magArray = vtkSmartPointer<vtkFloatArray>::New();
        magArray->SetName("VelocityMagnitude");
        magArray->SetNumberOfTuples(output->GetNumberOfPoints());

        auto* vectors = output->GetPointData()->GetVectors();
        if (vectors) {
            for (vtkIdType i = 0; i < output->GetNumberOfPoints(); ++i) {
                double v[3];
                vectors->GetTuple(i, v);
                magArray->SetValue(i, static_cast<float>(
                    std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])));
            }
        } else {
            magArray->FillComponent(0, 0.0);
        }

        output->GetPointData()->SetScalars(magArray);
    }

    return output;
}

// =============================================================================
// Static: Compute LIC texture
// =============================================================================

std::expected<vtkSmartPointer<vtkImageData>, OverlayError>
StreamlineOverlayRenderer::computeLIC(
    vtkSmartPointer<vtkImageData> velocitySlice,
    const LICParams& params) {

    if (!velocitySlice) {
        return std::unexpected(OverlayError::NoScalarField);
    }

    int* dims = velocitySlice->GetDimensions();
    double* spacing = velocitySlice->GetSpacing();
    double* origin = velocitySlice->GetOrigin();

    int width = dims[0];
    int height = dims[1];

    if (width < 2 || height < 2) {
        return std::unexpected(OverlayError::InternalError);
    }

    // Generate white noise texture
    std::mt19937 rng(params.noiseSeed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    std::vector<float> noise(static_cast<size_t>(width) * height);
    for (auto& n : noise) {
        n = dist(rng);
    }

    // Get velocity data pointer
    auto* velPtr = static_cast<float*>(velocitySlice->GetScalarPointer());
    int numComponents = velocitySlice->GetNumberOfScalarComponents();

    // LIC output
    std::vector<float> licOutput(static_cast<size_t>(width) * height, 0.0f);

    // For each pixel, trace streamline forward and backward,
    // average noise values along the path
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            double sum = 0.0;
            int count = 0;

            // Trace forward and backward
            for (int dir = -1; dir <= 1; dir += 2) {
                double px = static_cast<double>(x);
                double py = static_cast<double>(y);

                for (int step = 0; step < params.kernelLength; ++step) {
                    int ix = static_cast<int>(std::round(px));
                    int iy = static_cast<int>(std::round(py));

                    if (ix < 0 || ix >= width || iy < 0 || iy >= height) {
                        break;
                    }

                    size_t idx = static_cast<size_t>(iy) * width + ix;
                    sum += noise[idx];
                    ++count;

                    // Get velocity at current position (bilinear would be better,
                    // but nearest-neighbor is sufficient for visualization)
                    size_t velIdx = idx * numComponents;
                    double vx = velPtr[velIdx];
                    double vy = velPtr[velIdx + 1];

                    double mag = std::sqrt(vx * vx + vy * vy);
                    if (mag < 1e-10) {
                        break;
                    }

                    // Normalize and advance
                    px += dir * params.stepSize * vx / mag;
                    py += dir * params.stepSize * vy / mag;
                }
            }

            size_t outIdx = static_cast<size_t>(y) * width + x;
            licOutput[outIdx] = (count > 0)
                ? static_cast<float>(sum / count)
                : noise[outIdx];
        }
    }

    // Create output vtkImageData
    auto output = vtkSmartPointer<vtkImageData>::New();
    output->SetDimensions(width, height, 1);
    output->SetSpacing(spacing[0], spacing[1], 1.0);
    output->SetOrigin(origin[0], origin[1], 0.0);
    output->AllocateScalars(VTK_UNSIGNED_CHAR, 1);

    auto* outPtr = static_cast<unsigned char*>(output->GetScalarPointer());
    for (size_t i = 0; i < licOutput.size(); ++i) {
        outPtr[i] = static_cast<unsigned char>(
            std::clamp(licOutput[i] * 255.0f, 0.0f, 255.0f));
    }

    return output;
}

} // namespace dicom_viewer::services

#include "services/flow/flow_visualizer.hpp"

#include <algorithm>
#include <cmath>
#include <format>

#include <vtkArrowSource.h>
#include <vtkCellArray.h>
#include <vtkFloatArray.h>
#include <vtkGlyph3D.h>
#include <vtkImageData.h>
#include <vtkLookupTable.h>
#include <vtkPointData.h>
#include <vtkPointSource.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyLine.h>
#include <vtkRungeKutta45.h>
#include <vtkStreamTracer.h>
#include <vtkTubeFilter.h>

#include <kcenon/common/logging/log_macros.h>

namespace dicom_viewer::services {

// =============================================================================
// FlowVisualizer::Impl
// =============================================================================

class FlowVisualizer::Impl {
public:
    vtkSmartPointer<vtkImageData> velocityData;
    SeedRegion seed;
    ColorMode colorMode_ = ColorMode::VelocityMagnitude;
    double velocityMin = 0.0;
    double velocityMax = 100.0;  // cm/s default
    bool hasField = false;

    vtkSmartPointer<vtkPointSource> createSeedSource(int numPoints) const {
        auto source = vtkSmartPointer<vtkPointSource>::New();
        source->SetNumberOfPoints(numPoints);

        if (seed.type == SeedRegion::Type::Volume && velocityData) {
            double center[3] = {
                (seed.bounds[0] + seed.bounds[1]) / 2.0,
                (seed.bounds[2] + seed.bounds[3]) / 2.0,
                (seed.bounds[4] + seed.bounds[5]) / 2.0
            };
            double radius = std::max({
                seed.bounds[1] - seed.bounds[0],
                seed.bounds[3] - seed.bounds[2],
                seed.bounds[5] - seed.bounds[4]
            }) / 2.0;
            source->SetCenter(center);
            source->SetRadius(std::max(radius, 1.0));
        } else if (seed.type == SeedRegion::Type::Plane) {
            source->SetCenter(seed.planeOrigin.data());
            source->SetRadius(seed.planeRadius);
        } else if (velocityData) {
            // Fallback: use image data bounds
            double bounds[6];
            velocityData->GetBounds(bounds);
            double center[3] = {
                (bounds[0] + bounds[1]) / 2.0,
                (bounds[2] + bounds[3]) / 2.0,
                (bounds[4] + bounds[5]) / 2.0
            };
            double radius = std::max({
                bounds[1] - bounds[0],
                bounds[3] - bounds[2],
                bounds[5] - bounds[4]
            }) / 2.0;
            source->SetCenter(center);
            source->SetRadius(std::max(radius, 1.0));
        }

        source->Update();
        return source;
    }
};

// =============================================================================
// Lifecycle
// =============================================================================

FlowVisualizer::FlowVisualizer()
    : impl_(std::make_unique<Impl>()) {}

FlowVisualizer::~FlowVisualizer() = default;

FlowVisualizer::FlowVisualizer(FlowVisualizer&&) noexcept = default;
FlowVisualizer& FlowVisualizer::operator=(FlowVisualizer&&) noexcept = default;

// =============================================================================
// Data setup
// =============================================================================

std::expected<void, FlowError>
FlowVisualizer::setVelocityField(const VelocityPhase& phase) {
    auto vtkData = velocityFieldToVTK(phase);
    if (!vtkData) {
        return std::unexpected(vtkData.error());
    }

    impl_->velocityData = vtkData.value();
    impl_->hasField = true;

    // Auto-set seed region to image bounds if not configured
    bool boundsEmpty = (impl_->seed.bounds == std::array<double, 6>{0, 0, 0, 0, 0, 0});
    if (boundsEmpty) {
        double bounds[6];
        impl_->velocityData->GetBounds(bounds);
        for (int i = 0; i < 6; ++i) {
            impl_->seed.bounds[i] = bounds[i];
        }
    }

    LOG_DEBUG(std::format("Velocity field set for phase {}", phase.phaseIndex));
    return {};
}

void FlowVisualizer::setSeedRegion(const SeedRegion& region) {
    impl_->seed = region;
}

// =============================================================================
// Streamline generation
// =============================================================================

std::expected<vtkSmartPointer<vtkPolyData>, FlowError>
FlowVisualizer::generateStreamlines(const StreamlineParams& params) const {
    if (!impl_->hasField || !impl_->velocityData) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "No velocity field set for streamline generation"});
    }

    // Create seed points
    auto seedSource = impl_->createSeedSource(params.maxSeedPoints);

    // Configure stream tracer with RK45 integrator
    auto tracer = vtkSmartPointer<vtkStreamTracer>::New();
    tracer->SetInputData(impl_->velocityData);
    tracer->SetSourceConnection(seedSource->GetOutputPort());

    auto integrator = vtkSmartPointer<vtkRungeKutta45>::New();
    tracer->SetIntegrator(integrator);

    tracer->SetMaximumPropagation(params.maxSteps * params.stepLength);
    tracer->SetInitialIntegrationStep(params.stepLength);
    tracer->SetIntegrationDirectionToBoth();
    tracer->SetTerminalSpeed(params.terminalSpeed);
    tracer->SetMaximumNumberOfSteps(params.maxSteps);
    tracer->Update();

    // Apply tube filter for 3D appearance
    auto tubeFilter = vtkSmartPointer<vtkTubeFilter>::New();
    tubeFilter->SetInputConnection(tracer->GetOutputPort());
    tubeFilter->SetRadius(params.tubeRadius);
    tubeFilter->SetNumberOfSides(params.tubeSides);
    tubeFilter->CappingOn();
    tubeFilter->Update();

    auto output = vtkSmartPointer<vtkPolyData>::New();
    output->DeepCopy(tubeFilter->GetOutput());

    LOG_INFO(std::format("Streamlines: {} cells, {} points",
                         output->GetNumberOfCells(),
                         output->GetNumberOfPoints()));
    return output;
}

// =============================================================================
// Pathline generation (manual Euler integration across phases)
// =============================================================================

std::expected<vtkSmartPointer<vtkPolyData>, FlowError>
FlowVisualizer::generatePathlines(const std::vector<VelocityPhase>& allPhases,
                                  const PathlineParams& params) const {
    if (allPhases.empty()) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "No phases provided for pathline generation"});
    }

    // Convert all phases to VTK
    std::vector<vtkSmartPointer<vtkImageData>> phaseData;
    phaseData.reserve(allPhases.size());
    for (const auto& phase : allPhases) {
        auto vtkResult = velocityFieldToVTK(phase);
        if (!vtkResult) {
            return std::unexpected(vtkResult.error());
        }
        phaseData.push_back(vtkResult.value());
    }

    // Create seed points
    auto seedSource = impl_->createSeedSource(params.maxSeedPoints);
    auto seedPoints = seedSource->GetOutput()->GetPoints();
    if (!seedPoints || seedPoints->GetNumberOfPoints() == 0) {
        return std::unexpected(FlowError{
            FlowError::Code::InternalError,
            "Failed to generate seed points"});
    }

    auto output = vtkSmartPointer<vtkPolyData>::New();
    auto points = vtkSmartPointer<vtkPoints>::New();
    auto lines = vtkSmartPointer<vtkCellArray>::New();

    auto timeScalars = vtkSmartPointer<vtkFloatArray>::New();
    timeScalars->SetName("TriggerTime");

    auto magnitudeScalars = vtkSmartPointer<vtkFloatArray>::New();
    magnitudeScalars->SetName("VelocityMagnitude");

    int numSeeds = static_cast<int>(seedPoints->GetNumberOfPoints());
    for (int s = 0; s < numSeeds; ++s) {
        double pos[3];
        seedPoints->GetPoint(s, pos);

        auto polyLine = vtkSmartPointer<vtkPolyLine>::New();
        bool active = true;

        for (size_t p = 0; p < phaseData.size() && active; ++p) {
            auto* imageData = phaseData[p].Get();
            int dims[3];
            imageData->GetDimensions(dims);

            // Check if position is within image bounds
            int ijk[3];
            double pcoords[3];
            int inBounds = imageData->ComputeStructuredCoordinates(pos, ijk, pcoords);
            if (!inBounds) {
                break;
            }

            auto* vectors = imageData->GetPointData()->GetVectors();
            if (!vectors) break;

            vtkIdType ptId = ijk[0] + dims[0] * (ijk[1] + dims[1] * ijk[2]);
            double vel[3];
            vectors->GetTuple(ptId, vel);

            double mag = std::sqrt(vel[0] * vel[0] + vel[1] * vel[1] + vel[2] * vel[2]);
            if (mag < params.terminalSpeed) {
                break;
            }

            // Record current position
            vtkIdType pid = points->InsertNextPoint(pos);
            polyLine->GetPointIds()->InsertNextId(pid);
            timeScalars->InsertNextValue(
                static_cast<float>(allPhases[p].triggerTime));
            magnitudeScalars->InsertNextValue(static_cast<float>(mag));

            // Advance position with Euler integration
            // Scale velocity by spacing to convert cm/s to mm displacement
            double spacing[3];
            imageData->GetSpacing(spacing);
            pos[0] += vel[0] * spacing[0];
            pos[1] += vel[1] * spacing[1];
            pos[2] += vel[2] * spacing[2];
        }

        if (polyLine->GetPointIds()->GetNumberOfIds() >= 2) {
            lines->InsertNextCell(polyLine);
        }
    }

    output->SetPoints(points);
    output->SetLines(lines);
    output->GetPointData()->AddArray(timeScalars);
    output->GetPointData()->AddArray(magnitudeScalars);

    LOG_INFO(std::format("Pathlines: {} lines, {} points from {} seeds",
                         output->GetNumberOfCells(),
                         output->GetNumberOfPoints(), numSeeds));
    return output;
}

// =============================================================================
// Vector glyph generation
// =============================================================================

std::expected<vtkSmartPointer<vtkPolyData>, FlowError>
FlowVisualizer::generateGlyphs(const GlyphParams& params) const {
    if (!impl_->hasField || !impl_->velocityData) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "No velocity field set for glyph generation"});
    }

    auto* imageData = impl_->velocityData.Get();
    int dims[3];
    imageData->GetDimensions(dims);

    auto* vectors = imageData->GetPointData()->GetVectors();
    if (!vectors) {
        return std::unexpected(FlowError{
            FlowError::Code::InternalError,
            "Velocity data has no vector field"});
    }

    // Subsample the velocity field
    auto sampledPoints = vtkSmartPointer<vtkPoints>::New();
    auto sampledVectors = vtkSmartPointer<vtkFloatArray>::New();
    sampledVectors->SetNumberOfComponents(3);
    sampledVectors->SetName("velocity");

    auto magnitudes = vtkSmartPointer<vtkFloatArray>::New();
    magnitudes->SetName("VelocityMagnitude");

    int skip = std::max(1, params.skipFactor);
    for (int z = 0; z < dims[2]; z += skip) {
        for (int y = 0; y < dims[1]; y += skip) {
            for (int x = 0; x < dims[0]; x += skip) {
                vtkIdType ptId = x + dims[0] * (y + dims[1] * z);

                double vel[3];
                vectors->GetTuple(ptId, vel);

                double mag = std::sqrt(vel[0] * vel[0] + vel[1] * vel[1] + vel[2] * vel[2]);
                if (mag < params.minMagnitude) continue;

                double pt[3];
                imageData->GetPoint(ptId, pt);

                sampledPoints->InsertNextPoint(pt);
                sampledVectors->InsertNextTuple(vel);
                magnitudes->InsertNextValue(static_cast<float>(mag));
            }
        }
    }

    auto sampledPolyData = vtkSmartPointer<vtkPolyData>::New();
    sampledPolyData->SetPoints(sampledPoints);
    sampledPolyData->GetPointData()->SetVectors(sampledVectors);
    sampledPolyData->GetPointData()->SetScalars(magnitudes);

    // Create arrow glyph source
    auto arrowSource = vtkSmartPointer<vtkArrowSource>::New();
    arrowSource->SetTipResolution(8);
    arrowSource->SetShaftResolution(8);

    // Apply glyph filter
    auto glyphFilter = vtkSmartPointer<vtkGlyph3D>::New();
    glyphFilter->SetInputData(sampledPolyData);
    glyphFilter->SetSourceConnection(arrowSource->GetOutputPort());
    glyphFilter->SetScaleFactor(params.scaleFactor);
    glyphFilter->OrientOn();
    glyphFilter->SetVectorModeToUseVector();
    glyphFilter->SetScaleModeToScaleByVector();
    glyphFilter->Update();

    auto output = vtkSmartPointer<vtkPolyData>::New();
    output->DeepCopy(glyphFilter->GetOutput());

    LOG_INFO(std::format("Glyphs: {} cells from {} sampled points",
                         output->GetNumberOfCells(),
                         sampledPoints->GetNumberOfPoints()));
    return output;
}

// =============================================================================
// Color mapping
// =============================================================================

void FlowVisualizer::setColorMode(ColorMode mode) {
    impl_->colorMode_ = mode;
}

void FlowVisualizer::setVelocityRange(double minVel, double maxVel) {
    impl_->velocityMin = minVel;
    impl_->velocityMax = maxVel;
}

vtkSmartPointer<vtkLookupTable>
FlowVisualizer::createLookupTable() const {
    auto lut = vtkSmartPointer<vtkLookupTable>::New();
    lut->SetNumberOfTableValues(256);

    switch (impl_->colorMode_) {
        case ColorMode::VelocityMagnitude:
            // Rainbow: blue (low) → red (high)
            lut->SetRange(impl_->velocityMin, impl_->velocityMax);
            lut->SetHueRange(0.667, 0.0);
            lut->SetSaturationRange(1.0, 1.0);
            lut->SetValueRange(1.0, 1.0);
            break;

        case ColorMode::VelocityComponent:
            // Diverging: blue → white → red
            lut->SetRange(-impl_->velocityMax, impl_->velocityMax);
            for (int i = 0; i < 256; ++i) {
                double t = i / 255.0;
                double r, g, b;
                if (t < 0.5) {
                    double s = t * 2.0;
                    r = s; g = s; b = 1.0;
                } else {
                    double s = (t - 0.5) * 2.0;
                    r = 1.0; g = 1.0 - s; b = 1.0 - s;
                }
                lut->SetTableValue(i, r, g, b, 1.0);
            }
            break;

        case ColorMode::FlowDirection:
            // RGB direction encoding
            lut->SetRange(0.0, 1.0);
            for (int i = 0; i < 256; ++i) {
                double t = i / 255.0;
                lut->SetTableValue(i, t, 1.0 - t, 0.5, 1.0);
            }
            break;

        case ColorMode::TriggerTime:
            // Viridis-like sequential: dark purple → yellow
            lut->SetRange(0.0, 1000.0);
            for (int i = 0; i < 256; ++i) {
                double t = i / 255.0;
                double r = 0.267 + t * (0.993 - 0.267);
                double g = 0.004 + t * (0.906 - 0.004);
                double b = 0.329 + t * (0.143 - 0.329);
                lut->SetTableValue(i, r, g, b, 1.0);
            }
            break;
    }

    lut->Build();
    return lut;
}

// =============================================================================
// State queries
// =============================================================================

bool FlowVisualizer::hasVelocityField() const {
    return impl_->hasField;
}

ColorMode FlowVisualizer::colorMode() const {
    return impl_->colorMode_;
}

SeedRegion FlowVisualizer::seedRegion() const {
    return impl_->seed;
}

// =============================================================================
// ITK VectorImage → VTK ImageData conversion
// =============================================================================

std::expected<vtkSmartPointer<vtkImageData>, FlowError>
FlowVisualizer::velocityFieldToVTK(const VelocityPhase& phase) {
    if (!phase.velocityField) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "VelocityPhase has null velocity field"});
    }

    auto itkImage = phase.velocityField;
    auto region = itkImage->GetLargestPossibleRegion();
    auto size = region.GetSize();
    auto spacing = itkImage->GetSpacing();
    auto origin = itkImage->GetOrigin();

    if (itkImage->GetNumberOfComponentsPerPixel() != 3) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "Expected 3-component vector image, got " +
                std::to_string(itkImage->GetNumberOfComponentsPerPixel())});
    }

    auto vtkImage = vtkSmartPointer<vtkImageData>::New();
    vtkImage->SetDimensions(
        static_cast<int>(size[0]),
        static_cast<int>(size[1]),
        static_cast<int>(size[2]));
    vtkImage->SetSpacing(spacing[0], spacing[1], spacing[2]);
    vtkImage->SetOrigin(origin[0], origin[1], origin[2]);

    vtkIdType numPoints = static_cast<vtkIdType>(size[0] * size[1] * size[2]);

    // Copy velocity vectors: ITK VectorImage buffer is interleaved [Vx,Vy,Vz,...]
    auto velocityArray = vtkSmartPointer<vtkFloatArray>::New();
    velocityArray->SetNumberOfComponents(3);
    velocityArray->SetName("velocity");
    velocityArray->SetNumberOfTuples(numPoints);

    auto* itkBuffer = itkImage->GetBufferPointer();
    for (vtkIdType i = 0; i < numPoints; ++i) {
        velocityArray->SetTuple3(i,
            itkBuffer[i * 3],
            itkBuffer[i * 3 + 1],
            itkBuffer[i * 3 + 2]);
    }
    vtkImage->GetPointData()->SetVectors(velocityArray);

    // Add velocity magnitude as scalar data
    auto magnitudeArray = vtkSmartPointer<vtkFloatArray>::New();
    magnitudeArray->SetName("VelocityMagnitude");
    magnitudeArray->SetNumberOfTuples(numPoints);

    for (vtkIdType i = 0; i < numPoints; ++i) {
        float vx = itkBuffer[i * 3];
        float vy = itkBuffer[i * 3 + 1];
        float vz = itkBuffer[i * 3 + 2];
        magnitudeArray->SetValue(i, std::sqrt(vx * vx + vy * vy + vz * vz));
    }
    vtkImage->GetPointData()->SetScalars(magnitudeArray);

    return vtkImage;
}

}  // namespace dicom_viewer::services

#include "services/cardiac/curved_planar_reformatter.hpp"
#include "core/logging.hpp"

#include <algorithm>
#include <cmath>

#include <itkImageRegionConstIterator.h>
#include <itkLinearInterpolateImageFunction.h>

namespace dicom_viewer::services {

using ImageType = itk::Image<short, 3>;
using FloatImageType = itk::Image<float, 3>;
using InterpolatorType = itk::LinearInterpolateImageFunction<ImageType, double>;

class CurvedPlanarReformatter::Impl {
public:
    std::shared_ptr<spdlog::logger> logger;

    Impl() : logger(logging::LoggerFactory::create("CurvedPlanarReformatter")) {}

    // Interpolate point along centerline at given arc length fraction
    static CenterlinePoint interpolateAlongCenterline(
        const std::vector<CenterlinePoint>& points,
        const std::vector<double>& arcLengths,
        double targetArcLen);

    // Sample a value from the ITK image at physical coordinates
    static double sampleVolume(InterpolatorType::Pointer interp,
                               const std::array<double, 3>& pos);
};

CurvedPlanarReformatter::CurvedPlanarReformatter()
    : impl_(std::make_unique<Impl>()) {}

CurvedPlanarReformatter::~CurvedPlanarReformatter() = default;

CurvedPlanarReformatter::CurvedPlanarReformatter(CurvedPlanarReformatter&&) noexcept = default;
CurvedPlanarReformatter& CurvedPlanarReformatter::operator=(CurvedPlanarReformatter&&) noexcept = default;

// =============================================================================
// Helper Functions
// =============================================================================

CenterlinePoint CurvedPlanarReformatter::Impl::interpolateAlongCenterline(
    const std::vector<CenterlinePoint>& points,
    const std::vector<double>& arcLengths,
    double targetArcLen)
{
    // Find bracketing segment
    int lo = 0;
    for (int i = 1; i < static_cast<int>(arcLengths.size()); ++i) {
        if (arcLengths[i] >= targetArcLen) {
            lo = i - 1;
            break;
        }
        lo = i;
    }
    int hi = std::min(lo + 1, static_cast<int>(points.size()) - 1);

    double segLen = arcLengths[hi] - arcLengths[lo];
    double t = (segLen > 1e-10) ? (targetArcLen - arcLengths[lo]) / segLen : 0.0;
    t = std::clamp(t, 0.0, 1.0);

    CenterlinePoint result;
    for (int d = 0; d < 3; ++d) {
        result.position[d] = points[lo].position[d]
                           + t * (points[hi].position[d] - points[lo].position[d]);
        result.tangent[d] = points[lo].tangent[d]
                          + t * (points[hi].tangent[d] - points[lo].tangent[d]);
        result.normal[d] = points[lo].normal[d]
                         + t * (points[hi].normal[d] - points[lo].normal[d]);
    }
    result.radius = points[lo].radius + t * (points[hi].radius - points[lo].radius);

    // Re-normalize tangent and normal
    double tMag = std::sqrt(result.tangent[0]*result.tangent[0]
                          + result.tangent[1]*result.tangent[1]
                          + result.tangent[2]*result.tangent[2]);
    if (tMag > 1e-10) {
        for (auto& v : result.tangent) v /= tMag;
    }

    double nMag = std::sqrt(result.normal[0]*result.normal[0]
                          + result.normal[1]*result.normal[1]
                          + result.normal[2]*result.normal[2]);
    if (nMag > 1e-10) {
        for (auto& v : result.normal) v /= nMag;
    }

    return result;
}

double CurvedPlanarReformatter::Impl::sampleVolume(
    InterpolatorType::Pointer interp,
    const std::array<double, 3>& pos)
{
    InterpolatorType::PointType point;
    point[0] = pos[0];
    point[1] = pos[1];
    point[2] = pos[2];

    if (interp->IsInsideBuffer(point)) {
        return interp->Evaluate(point);
    }
    return -1024.0;  // Air HU value as default
}

// =============================================================================
// Straightened CPR
// =============================================================================

std::expected<vtkSmartPointer<vtkImageData>, CardiacError>
CurvedPlanarReformatter::generateStraightenedCPR(
    const CenterlineResult& centerline,
    ImageType::Pointer volume,
    double samplingWidth,
    double samplingResolution) const
{
    if (!centerline.isValid()) {
        return std::unexpected(CardiacError{
            CardiacError::Code::InternalError,
            "Invalid centerline for CPR generation"
        });
    }

    if (!volume) {
        return std::unexpected(CardiacError{
            CardiacError::Code::InternalError,
            "Null volume pointer for CPR generation"
        });
    }

    impl_->logger->info("Generating straightened CPR: width={:.1f}mm, res={:.2f}mm",
                         samplingWidth, samplingResolution);

    const auto& points = centerline.points;
    int n = static_cast<int>(points.size());

    // Compute arc lengths
    std::vector<double> arcLengths(n, 0.0);
    for (int i = 1; i < n; ++i) {
        double dx = points[i].position[0] - points[i-1].position[0];
        double dy = points[i].position[1] - points[i-1].position[1];
        double dz = points[i].position[2] - points[i-1].position[2];
        arcLengths[i] = arcLengths[i-1] + std::sqrt(dx*dx + dy*dy + dz*dz);
    }
    double totalLength = arcLengths.back();

    // Create interpolator for volume sampling
    auto interp = InterpolatorType::New();
    interp->SetInputImage(volume);

    // Output dimensions
    int widthPixels = static_cast<int>(2.0 * samplingWidth / samplingResolution) + 1;
    int heightPixels = static_cast<int>(totalLength / samplingResolution) + 1;

    auto cprImage = vtkSmartPointer<vtkImageData>::New();
    cprImage->SetDimensions(widthPixels, heightPixels, 1);
    cprImage->SetSpacing(samplingResolution, samplingResolution, 1.0);
    cprImage->AllocateScalars(VTK_SHORT, 1);

    short* pixels = static_cast<short*>(cprImage->GetScalarPointer());
    std::fill(pixels, pixels + widthPixels * heightPixels, static_cast<short>(-1024));

    // Sample volume along centerline
    for (int row = 0; row < heightPixels; ++row) {
        double arcLen = row * samplingResolution;
        if (arcLen > totalLength) break;

        auto pt = Impl::interpolateAlongCenterline(points, arcLengths, arcLen);

        // Compute binormal = tangent × normal
        std::array<double, 3> binormal;
        binormal[0] = pt.tangent[1]*pt.normal[2] - pt.tangent[2]*pt.normal[1];
        binormal[1] = pt.tangent[2]*pt.normal[0] - pt.tangent[0]*pt.normal[2];
        binormal[2] = pt.tangent[0]*pt.normal[1] - pt.tangent[1]*pt.normal[0];

        for (int col = 0; col < widthPixels; ++col) {
            double offset = (col - widthPixels / 2) * samplingResolution;

            std::array<double, 3> samplePos;
            for (int d = 0; d < 3; ++d) {
                samplePos[d] = pt.position[d] + offset * pt.normal[d];
            }

            double value = Impl::sampleVolume(interp, samplePos);
            pixels[row * widthPixels + col] = static_cast<short>(std::round(value));
        }
    }

    impl_->logger->info("Straightened CPR generated: {}x{} pixels",
                         widthPixels, heightPixels);
    return cprImage;
}

// =============================================================================
// Cross-Sectional CPR
// =============================================================================

std::expected<std::vector<vtkSmartPointer<vtkImageData>>, CardiacError>
CurvedPlanarReformatter::generateCrossSectionalCPR(
    const CenterlineResult& centerline,
    ImageType::Pointer volume,
    double interval,
    double crossSectionSize,
    double samplingResolution) const
{
    if (!centerline.isValid()) {
        return std::unexpected(CardiacError{
            CardiacError::Code::InternalError,
            "Invalid centerline for cross-sectional CPR"
        });
    }

    if (!volume) {
        return std::unexpected(CardiacError{
            CardiacError::Code::InternalError,
            "Null volume pointer for cross-sectional CPR"
        });
    }

    const auto& points = centerline.points;
    int n = static_cast<int>(points.size());

    // Compute arc lengths
    std::vector<double> arcLengths(n, 0.0);
    for (int i = 1; i < n; ++i) {
        double dx = points[i].position[0] - points[i-1].position[0];
        double dy = points[i].position[1] - points[i-1].position[1];
        double dz = points[i].position[2] - points[i-1].position[2];
        arcLengths[i] = arcLengths[i-1] + std::sqrt(dx*dx + dy*dy + dz*dz);
    }
    double totalLength = arcLengths.back();

    int numSections = static_cast<int>(totalLength / interval) + 1;
    int sectionPixels = static_cast<int>(2.0 * crossSectionSize / samplingResolution) + 1;

    impl_->logger->info("Generating {} cross-sections (interval={:.1f}mm, size={}x{} pixels)",
                         numSections, interval, sectionPixels, sectionPixels);

    auto interp = InterpolatorType::New();
    interp->SetInputImage(volume);

    std::vector<vtkSmartPointer<vtkImageData>> sections;
    sections.reserve(numSections);

    for (int s = 0; s < numSections; ++s) {
        double arcLen = s * interval;
        if (arcLen > totalLength) break;

        auto pt = Impl::interpolateAlongCenterline(points, arcLengths, arcLen);

        // Compute binormal
        std::array<double, 3> binormal;
        binormal[0] = pt.tangent[1]*pt.normal[2] - pt.tangent[2]*pt.normal[1];
        binormal[1] = pt.tangent[2]*pt.normal[0] - pt.tangent[0]*pt.normal[2];
        binormal[2] = pt.tangent[0]*pt.normal[1] - pt.tangent[1]*pt.normal[0];

        auto section = vtkSmartPointer<vtkImageData>::New();
        section->SetDimensions(sectionPixels, sectionPixels, 1);
        section->SetSpacing(samplingResolution, samplingResolution, 1.0);
        section->AllocateScalars(VTK_SHORT, 1);

        short* pixels = static_cast<short*>(section->GetScalarPointer());
        int halfSize = sectionPixels / 2;

        for (int row = 0; row < sectionPixels; ++row) {
            double offsetY = (row - halfSize) * samplingResolution;
            for (int col = 0; col < sectionPixels; ++col) {
                double offsetX = (col - halfSize) * samplingResolution;

                std::array<double, 3> samplePos;
                for (int d = 0; d < 3; ++d) {
                    samplePos[d] = pt.position[d]
                                 + offsetX * pt.normal[d]
                                 + offsetY * binormal[d];
                }

                double value = Impl::sampleVolume(interp, samplePos);
                pixels[row * sectionPixels + col] = static_cast<short>(std::round(value));
            }
        }

        sections.push_back(section);
    }

    impl_->logger->info("Generated {} cross-sectional CPR views", sections.size());
    return sections;
}

// =============================================================================
// Stretched CPR
// =============================================================================

std::expected<vtkSmartPointer<vtkImageData>, CardiacError>
CurvedPlanarReformatter::generateStretchedCPR(
    const CenterlineResult& centerline,
    ImageType::Pointer volume,
    double samplingWidth,
    double samplingResolution) const
{
    if (!centerline.isValid()) {
        return std::unexpected(CardiacError{
            CardiacError::Code::InternalError,
            "Invalid centerline for stretched CPR"
        });
    }

    if (!volume) {
        return std::unexpected(CardiacError{
            CardiacError::Code::InternalError,
            "Null volume pointer for stretched CPR"
        });
    }

    impl_->logger->info("Generating stretched CPR: width={:.1f}mm, res={:.2f}mm",
                         samplingWidth, samplingResolution);

    const auto& points = centerline.points;
    int n = static_cast<int>(points.size());

    // Compute arc lengths
    std::vector<double> arcLengths(n, 0.0);
    for (int i = 1; i < n; ++i) {
        double dx = points[i].position[0] - points[i-1].position[0];
        double dy = points[i].position[1] - points[i-1].position[1];
        double dz = points[i].position[2] - points[i-1].position[2];
        arcLengths[i] = arcLengths[i-1] + std::sqrt(dx*dx + dy*dy + dz*dz);
    }
    double totalLength = arcLengths.back();

    // Create interpolator
    auto interp = InterpolatorType::New();
    interp->SetInputImage(volume);

    // For stretched CPR, we sample at each original centerline point
    // but space the output rows proportionally to arc length
    int widthPixels = static_cast<int>(2.0 * samplingWidth / samplingResolution) + 1;
    int heightPixels = static_cast<int>(totalLength / samplingResolution) + 1;

    auto cprImage = vtkSmartPointer<vtkImageData>::New();
    cprImage->SetDimensions(widthPixels, heightPixels, 1);
    cprImage->SetSpacing(samplingResolution, samplingResolution, 1.0);
    cprImage->AllocateScalars(VTK_SHORT, 1);

    short* pixels = static_cast<short*>(cprImage->GetScalarPointer());
    std::fill(pixels, pixels + widthPixels * heightPixels, static_cast<short>(-1024));

    // Sample using proportional arc-length spacing
    for (int row = 0; row < heightPixels; ++row) {
        double arcLen = row * samplingResolution;
        if (arcLen > totalLength) break;

        auto pt = Impl::interpolateAlongCenterline(points, arcLengths, arcLen);

        // Rotate normal by vessel curvature for stretched view
        // Use a rotating frame that tracks the vessel twist
        std::array<double, 3> binormal;
        binormal[0] = pt.tangent[1]*pt.normal[2] - pt.tangent[2]*pt.normal[1];
        binormal[1] = pt.tangent[2]*pt.normal[0] - pt.tangent[0]*pt.normal[2];
        binormal[2] = pt.tangent[0]*pt.normal[1] - pt.tangent[1]*pt.normal[0];

        for (int col = 0; col < widthPixels; ++col) {
            double angle = M_PI * (col - widthPixels / 2) / (widthPixels / 2);
            double r = samplingWidth;

            // Sample in a rotated perpendicular plane
            std::array<double, 3> samplePos;
            for (int d = 0; d < 3; ++d) {
                double offsetN = r * std::cos(angle) * pt.normal[d];
                double offsetB = r * std::sin(angle) * binormal[d];
                // Scale by distance from center
                double distFromCenter = std::abs(col - widthPixels / 2)
                                      * samplingResolution;
                double scale = distFromCenter / (samplingWidth + 1e-10);
                samplePos[d] = pt.position[d]
                             + scale * (offsetN + offsetB);
            }

            double value = Impl::sampleVolume(interp, samplePos);
            pixels[row * widthPixels + col] = static_cast<short>(std::round(value));
        }
    }

    impl_->logger->info("Stretched CPR generated: {}x{} pixels", widthPixels, heightPixels);
    return cprImage;
}

}  // namespace dicom_viewer::services

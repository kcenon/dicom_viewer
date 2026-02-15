#pragma once

#include "services/export/data_exporter.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <itkImage.h>
#include <itkVectorImage.h>

namespace dicom_viewer::services {

/**
 * @brief Exporter for Ensight Gold format (.case/.geo/.variable)
 *
 * Produces a complete Ensight Gold file set from ITK image data,
 * compatible with Ansys Ensight and Paraview visualization tools.
 *
 * Output structure:
 * @code
 * output_dir/
 * ├── case_name.case              // ASCII index file
 * ├── case_name.geo               // C Binary geometry (structured grid)
 * ├── case_name.Magnitude0001     // Scalar variable, phase 1
 * ├── case_name.Velocity0001      // Vector variable, phase 1
 * └── ...
 * @endcode
 *
 * Usage:
 * @code
 * EnsightExporter exporter;
 *
 * EnsightExporter::PhaseData phase;
 * phase.timeValue = 0.0;
 * phase.scalars.push_back({"Magnitude", magnitudeImage});
 * phase.vectors.push_back({"Velocity", velocityField});
 *
 * EnsightExporter::ExportConfig config;
 * config.outputDir = "/path/to/output";
 * config.caseName = "flow_data";
 *
 * auto result = exporter.exportData({phase}, config);
 * @endcode
 *
 * @trace SRS-FR-046
 */
class EnsightExporter {
public:
    using FloatImage3D = itk::Image<float, 3>;
    using VectorImage3D = itk::VectorImage<float, 3>;
    using ProgressCallback = std::function<void(double progress,
                                                const std::string& status)>;

    /**
     * @brief Named scalar field for export
     */
    struct ScalarField {
        std::string name;                  ///< Variable name (e.g., "Magnitude")
        FloatImage3D::Pointer image;       ///< 3D scalar image
    };

    /**
     * @brief Named vector field for export
     */
    struct VectorField {
        std::string name;                  ///< Variable name (e.g., "Velocity")
        VectorImage3D::Pointer image;      ///< 3D vector image (3 components)
    };

    /**
     * @brief Data for a single temporal phase
     */
    struct PhaseData {
        std::vector<ScalarField> scalars;  ///< Scalar variables for this phase
        std::vector<VectorField> vectors;  ///< Vector variables for this phase
        double timeValue = 0.0;            ///< Time in seconds from R-wave
    };

    /**
     * @brief Export configuration
     */
    struct ExportConfig {
        std::filesystem::path outputDir;   ///< Output directory (must exist)
        std::string caseName = "flow_data"; ///< Base name for all files
    };

    EnsightExporter() = default;
    ~EnsightExporter() = default;

    /**
     * @brief Set progress callback for monitoring export
     * @param callback Function receiving (progress [0-1], status message)
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Export multi-phase data to Ensight Gold format
     *
     * All phases must have the same variable names and image dimensions.
     * Geometry is taken from the first scalar or vector image of phase 0.
     *
     * @param phases Temporal phase data (at least 1 phase)
     * @param config Export configuration
     * @return void on success, ExportError on failure
     */
    [[nodiscard]] std::expected<void, ExportError>
    exportData(const std::vector<PhaseData>& phases,
               const ExportConfig& config) const;

    // --- Low-level writers (public for testing) ---

    /**
     * @brief Write Ensight Gold case file (ASCII)
     *
     * @param path Output path for .case file
     * @param caseName Base name used in file references
     * @param scalarNames Names of scalar variables
     * @param vectorNames Names of vector variables
     * @param numTimeSteps Number of temporal phases
     * @param timeValues Time value for each phase (seconds)
     */
    [[nodiscard]] static std::expected<void, ExportError>
    writeCaseFile(const std::filesystem::path& path,
                  const std::string& caseName,
                  const std::vector<std::string>& scalarNames,
                  const std::vector<std::string>& vectorNames,
                  int numTimeSteps,
                  const std::vector<double>& timeValues);

    /**
     * @brief Write Ensight Gold geometry file (C Binary, structured grid)
     *
     * Generates node coordinates from image dimensions, spacing, and origin.
     *
     * @param path Output path for .geo file
     * @param referenceImage Image defining the grid geometry
     */
    [[nodiscard]] static std::expected<void, ExportError>
    writeGeometry(const std::filesystem::path& path,
                  const FloatImage3D* referenceImage);

    /**
     * @brief Write scalar variable file (C Binary, per node)
     *
     * @param path Output path for variable file
     * @param description Variable description (max 79 chars)
     * @param image Scalar image data
     */
    [[nodiscard]] static std::expected<void, ExportError>
    writeScalarVariable(const std::filesystem::path& path,
                        const std::string& description,
                        const FloatImage3D* image);

    /**
     * @brief Write vector variable file (C Binary, per node)
     *
     * @param path Output path for variable file
     * @param description Variable description (max 79 chars)
     * @param image Vector image data (must have 3 components)
     */
    [[nodiscard]] static std::expected<void, ExportError>
    writeVectorVariable(const std::filesystem::path& path,
                        const std::string& description,
                        const VectorImage3D* image);

private:
    /// Write an 80-byte padded string to a binary stream
    static void writeBinaryString(std::ofstream& out, const std::string& str);

    /// Write a 4-byte integer to a binary stream
    static void writeBinaryInt(std::ofstream& out, int32_t value);

    /// Write a 4-byte float to a binary stream
    static void writeBinaryFloat(std::ofstream& out, float value);

    /// Get image dimensions as array [nx, ny, nz]
    static std::array<int, 3> getImageDimensions(const FloatImage3D* image);

    ProgressCallback progressCallback_;
};

}  // namespace dicom_viewer::services

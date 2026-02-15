#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <itkImage.h>
#include <itkSmartPointer.h>

#include "services/segmentation/threshold_segmenter.hpp"

namespace dicom_viewer::services {

/**
 * @brief Available segmentation tools for manual drawing
 */
enum class SegmentationTool {
    None,         ///< No tool selected
    Brush,        ///< Draw with circular/square brush
    Eraser,       ///< Remove segmentation region
    Fill,         ///< Flood fill closed region
    Freehand,     ///< Draw freehand curve
    Polygon,      ///< Polygon ROI
    SmartScissors ///< Edge tracking (LiveWire)
};

/**
 * @brief Brush shape for drawing tools
 */
enum class BrushShape {
    Circle, ///< Circular brush
    Square  ///< Square brush
};

/**
 * @brief 2D point for mouse interaction
 */
struct Point2D {
    int x = 0;
    int y = 0;

    Point2D() = default;
    Point2D(int px, int py) : x(px), y(py) {}

    [[nodiscard]] bool operator==(const Point2D& other) const noexcept {
        return x == other.x && y == other.y;
    }

    [[nodiscard]] bool operator!=(const Point2D& other) const noexcept {
        return !(*this == other);
    }
};

/**
 * @brief Parameters for brush-based tools (Brush, Eraser)
 */
struct BrushParameters {
    /// Brush size in pixels (1-50)
    int size = 5;

    /// Brush shape
    BrushShape shape = BrushShape::Circle;

    /**
     * @brief Validate brush parameters
     * @return true if parameters are valid
     */
    [[nodiscard]] bool isValid() const noexcept {
        return size >= 1 && size <= 50;
    }
};

/**
 * @brief Parameters for fill tool
 */
struct FillParameters {
    /// Use 8-connectivity (true) or 4-connectivity (false)
    bool use8Connectivity = false;

    /// Tolerance for similar pixel values
    double tolerance = 0.0;
};

/**
 * @brief Parameters for polygon ROI tool
 */
struct PolygonParameters {
    /// Fill interior of completed polygon
    bool fillInterior = true;

    /// Draw polygon outline
    bool drawOutline = true;

    /// Minimum vertices required to complete polygon
    int minimumVertices = 3;

    /**
     * @brief Validate polygon parameters
     * @return true if parameters are valid
     */
    [[nodiscard]] bool isValid() const noexcept {
        return minimumVertices >= 3;
    }
};

/**
 * @brief Parameters for freehand drawing tool
 */
struct FreehandParameters {
    /// Enable path smoothing using Gaussian filter
    bool enableSmoothing = true;

    /// Smoothing window size (must be odd, 3-11)
    int smoothingWindowSize = 5;

    /// Enable path simplification using Douglas-Peucker algorithm
    bool enableSimplification = true;

    /// Simplification tolerance in pixels
    double simplificationTolerance = 2.0;

    /// Fill interior of closed path
    bool fillInterior = false;

    /// Distance threshold to auto-close path (pixels)
    double closeThreshold = 10.0;

    /**
     * @brief Validate freehand parameters
     * @return true if parameters are valid
     */
    [[nodiscard]] bool isValid() const noexcept {
        return smoothingWindowSize >= 3 && smoothingWindowSize <= 11 &&
               (smoothingWindowSize % 2 == 1) &&
               simplificationTolerance >= 0.0 &&
               closeThreshold >= 0.0;
    }
};

/**
 * @brief Parameters for Smart Scissors (LiveWire) tool
 *
 * Smart Scissors uses Dijkstra's algorithm to find the minimum cost path
 * along image edges between anchor points.
 */
struct SmartScissorsParameters {
    /// Weight for gradient magnitude in edge cost (0.0-1.0)
    double gradientWeight = 0.43;

    /// Weight for gradient direction in edge cost (0.0-1.0)
    double directionWeight = 0.43;

    /// Weight for Laplacian zero-crossing in edge cost (0.0-1.0)
    double laplacianWeight = 0.14;

    /// Gaussian sigma for gradient smoothing (1.0-5.0)
    double gaussianSigma = 1.5;

    /// Enable path smoothing after calculation
    bool enableSmoothing = true;

    /// Distance threshold to auto-close path when near start (pixels)
    double closeThreshold = 10.0;

    /// Fill interior when path is closed
    bool fillInterior = true;

    /**
     * @brief Validate Smart Scissors parameters
     * @return true if parameters are valid
     */
    [[nodiscard]] bool isValid() const noexcept {
        double totalWeight = gradientWeight + directionWeight + laplacianWeight;
        return gradientWeight >= 0.0 && gradientWeight <= 1.0 &&
               directionWeight >= 0.0 && directionWeight <= 1.0 &&
               laplacianWeight >= 0.0 && laplacianWeight <= 1.0 &&
               totalWeight > 0.0 && totalWeight <= 1.0 + 1e-6 &&
               gaussianSigma >= 1.0 && gaussianSigma <= 5.0 &&
               closeThreshold >= 0.0;
    }
};

/**
 * @brief Interactive controller for manual segmentation tools
 *
 * Provides drawing tools for manual segmentation on 2D slices including
 * brush, eraser, fill, freehand, polygon, and smart scissors tools.
 *
 * The controller manages mouse interactions and applies drawing operations
 * to a label map that stores the segmentation result.
 *
 * @example
 * @code
 * ManualSegmentationController controller;
 *
 * // Initialize with image dimensions
 * controller.initializeLabelMap(512, 512, 100);
 *
 * // Configure brush tool
 * controller.setActiveTool(SegmentationTool::Brush);
 * controller.setBrushSize(10);
 * controller.setBrushShape(BrushShape::Circle);
 * controller.setActiveLabel(1);
 *
 * // Handle mouse events
 * controller.onMousePress(Point2D{100, 100}, 50);
 * controller.onMouseMove(Point2D{110, 110}, 50);
 * controller.onMouseRelease(Point2D{120, 120}, 50);
 * @endcode
 *
 * @trace SRS-FR-023
 */
class ManualSegmentationController {
public:
    /// Label map type (2D slice for interactive drawing)
    using LabelMapType = itk::Image<uint8_t, 3>;

    /// 2D slice type for drawing operations
    using SliceType = itk::Image<uint8_t, 2>;

    /// Callback when label map is modified
    using ModificationCallback = std::function<void(int sliceIndex)>;

    /// Callback when undo/redo availability changes
    using UndoRedoCallback = std::function<void(bool canUndo, bool canRedo)>;

    ManualSegmentationController();
    ~ManualSegmentationController();

    // Non-copyable but movable
    ManualSegmentationController(const ManualSegmentationController&) = delete;
    ManualSegmentationController& operator=(const ManualSegmentationController&) = delete;
    ManualSegmentationController(ManualSegmentationController&&) noexcept;
    ManualSegmentationController& operator=(ManualSegmentationController&&) noexcept;

    /**
     * @brief Initialize the label map with given dimensions
     *
     * @param width Image width in pixels
     * @param height Image height in pixels
     * @param depth Number of slices (Z dimension)
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, SegmentationError>
    initializeLabelMap(int width, int height, int depth);

    /**
     * @brief Initialize with existing label map
     *
     * @param labelMap Existing label map to use
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, SegmentationError>
    setLabelMap(LabelMapType::Pointer labelMap);

    /**
     * @brief Get the current label map
     * @return Label map pointer or nullptr if not initialized
     */
    [[nodiscard]] LabelMapType::Pointer getLabelMap() const;

    /**
     * @brief Set the active segmentation tool
     * @param tool Tool to activate
     */
    void setActiveTool(SegmentationTool tool);

    /**
     * @brief Get the currently active tool
     * @return Current tool
     */
    [[nodiscard]] SegmentationTool getActiveTool() const noexcept;

    /**
     * @brief Set brush size for brush-based tools
     * @param size Brush size in pixels (1-50)
     * @return true if size was valid and set
     */
    bool setBrushSize(int size);

    /**
     * @brief Get current brush size
     * @return Brush size in pixels
     */
    [[nodiscard]] int getBrushSize() const noexcept;

    /**
     * @brief Set brush shape for brush-based tools
     * @param shape Brush shape (Circle or Square)
     */
    void setBrushShape(BrushShape shape);

    /**
     * @brief Get current brush shape
     * @return Brush shape
     */
    [[nodiscard]] BrushShape getBrushShape() const noexcept;

    /**
     * @brief Set brush parameters
     * @param params Brush parameters
     * @return true if parameters were valid and set
     */
    bool setBrushParameters(const BrushParameters& params);

    /**
     * @brief Get current brush parameters
     * @return Brush parameters
     */
    [[nodiscard]] BrushParameters getBrushParameters() const noexcept;

    /**
     * @brief Set fill parameters
     * @param params Fill parameters
     */
    void setFillParameters(const FillParameters& params);

    /**
     * @brief Get current fill parameters
     * @return Fill parameters
     */
    [[nodiscard]] FillParameters getFillParameters() const noexcept;

    /**
     * @brief Set polygon parameters
     * @param params Polygon parameters
     * @return true if parameters were valid and set
     */
    bool setPolygonParameters(const PolygonParameters& params);

    /**
     * @brief Get current polygon parameters
     * @return Polygon parameters
     */
    [[nodiscard]] PolygonParameters getPolygonParameters() const noexcept;

    /**
     * @brief Get the current polygon vertices
     *
     * Returns the vertices collected during polygon creation.
     *
     * @return Vector of polygon vertices
     */
    [[nodiscard]] std::vector<Point2D> getPolygonVertices() const;

    /**
     * @brief Undo the last polygon vertex
     *
     * Removes the most recently added vertex from the polygon.
     *
     * @return true if a vertex was removed, false if polygon is empty
     */
    bool undoLastPolygonVertex();

    /**
     * @brief Complete the current polygon
     *
     * Finalizes the polygon by connecting the last vertex to the first
     * and optionally filling the interior. Call this when the user
     * double-clicks or explicitly requests completion.
     *
     * @param sliceIndex Current slice index (Z)
     * @return true if polygon was completed, false if insufficient vertices
     */
    bool completePolygon(int sliceIndex);

    /**
     * @brief Check if polygon has enough vertices to complete
     * @return true if polygon can be completed
     */
    [[nodiscard]] bool canCompletePolygon() const noexcept;

    /**
     * @brief Set freehand parameters
     * @param params Freehand parameters
     * @return true if parameters were valid and set
     */
    bool setFreehandParameters(const FreehandParameters& params);

    /**
     * @brief Get current freehand parameters
     * @return Freehand parameters
     */
    [[nodiscard]] FreehandParameters getFreehandParameters() const noexcept;

    /**
     * @brief Get the current freehand path points
     *
     * Returns the path collected during freehand drawing.
     * The path may be simplified/smoothed based on parameters.
     *
     * @return Vector of path points
     */
    [[nodiscard]] std::vector<Point2D> getFreehandPath() const;

    /**
     * @brief Set Smart Scissors parameters
     * @param params Smart Scissors parameters
     * @return true if parameters were valid and set
     */
    bool setSmartScissorsParameters(const SmartScissorsParameters& params);

    /**
     * @brief Get current Smart Scissors parameters
     * @return Smart Scissors parameters
     */
    [[nodiscard]] SmartScissorsParameters getSmartScissorsParameters() const noexcept;

    /**
     * @brief Set source image for Smart Scissors edge computation
     *
     * The source image is used to compute edge cost map based on
     * gradient magnitude, direction, and Laplacian.
     *
     * @param image Source image (grayscale)
     * @param sliceIndex Slice index for 3D images
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, SegmentationError>
    setSmartScissorsSourceImage(itk::Image<float, 2>::Pointer image, int sliceIndex);

    /**
     * @brief Get the current Smart Scissors preview path
     *
     * Returns the calculated path from the last anchor to current mouse position.
     *
     * @return Vector of path points for preview
     */
    [[nodiscard]] std::vector<Point2D> getSmartScissorsPath() const;

    /**
     * @brief Get all anchor points for Smart Scissors
     * @return Vector of anchor points
     */
    [[nodiscard]] std::vector<Point2D> getSmartScissorsAnchors() const;

    /**
     * @brief Get the confirmed path segments (between anchors)
     * @return Vector of path points that have been confirmed
     */
    [[nodiscard]] std::vector<Point2D> getSmartScissorsConfirmedPath() const;

    /**
     * @brief Undo the last Smart Scissors anchor point
     * @return true if an anchor was removed
     */
    bool undoLastSmartScissorsAnchor();

    /**
     * @brief Complete Smart Scissors path and apply to label map
     *
     * Closes the path if near the starting point and fills the interior
     * based on parameters.
     *
     * @param sliceIndex Current slice index
     * @return true if path was completed successfully
     */
    bool completeSmartScissors(int sliceIndex);

    /**
     * @brief Check if Smart Scissors path can be completed
     * @return true if there are enough anchors to complete
     */
    [[nodiscard]] bool canCompleteSmartScissors() const noexcept;

    /**
     * @brief Set the active label ID for drawing
     * @param labelId Label value to draw (1-255, 0 reserved for background)
     * @return true if label ID was valid and set
     */
    bool setActiveLabel(uint8_t labelId);

    /**
     * @brief Get current active label ID
     * @return Active label ID
     */
    [[nodiscard]] uint8_t getActiveLabel() const noexcept;

    /**
     * @brief Handle mouse press event
     *
     * @param position Mouse position in image coordinates
     * @param sliceIndex Current slice index (Z)
     */
    void onMousePress(const Point2D& position, int sliceIndex);

    /**
     * @brief Handle mouse move event (while pressed)
     *
     * @param position Mouse position in image coordinates
     * @param sliceIndex Current slice index (Z)
     */
    void onMouseMove(const Point2D& position, int sliceIndex);

    /**
     * @brief Handle mouse release event
     *
     * @param position Mouse position in image coordinates
     * @param sliceIndex Current slice index (Z)
     */
    void onMouseRelease(const Point2D& position, int sliceIndex);

    /**
     * @brief Cancel current drawing operation
     */
    void cancelOperation();

    /**
     * @brief Check if a drawing operation is in progress
     * @return true if drawing is active
     */
    [[nodiscard]] bool isDrawing() const noexcept;

    /**
     * @brief Set callback for label map modifications
     * @param callback Callback function
     */
    void setModificationCallback(ModificationCallback callback);

    // -- Undo/Redo support --

    /**
     * @brief Undo the last segmentation operation
     * @return true if an undo was performed
     */
    bool undo();

    /**
     * @brief Redo the most recently undone operation
     * @return true if a redo was performed
     */
    bool redo();

    /**
     * @brief Check if undo is available
     */
    [[nodiscard]] bool canUndo() const noexcept;

    /**
     * @brief Check if redo is available
     */
    [[nodiscard]] bool canRedo() const noexcept;

    /**
     * @brief Set callback for undo/redo availability changes
     * @param callback Callback function
     */
    void setUndoRedoCallback(UndoRedoCallback callback);

    /**
     * @brief Clear all labels from the label map
     */
    void clearAll();

    /**
     * @brief Clear specific label from the label map
     * @param labelId Label to clear
     */
    void clearLabel(uint8_t labelId);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace dicom_viewer::services

#pragma once

#include <memory>
#include <QWidget>

#include "services/segmentation/manual_segmentation_controller.hpp"
#include "services/segmentation/segmentation_label.hpp"

namespace dicom_viewer::ui {

/**
 * @brief UI panel for manual segmentation tools
 *
 * Provides access to segmentation tools (Brush, Eraser, Fill, Freehand,
 * Polygon, Smart Scissors) with configurable parameters.
 *
 * @trace SRS-FR-023, PRD FR-015
 */
class SegmentationPanel : public QWidget {
    Q_OBJECT

public:
    explicit SegmentationPanel(QWidget* parent = nullptr);
    ~SegmentationPanel() override;

    // Non-copyable
    SegmentationPanel(const SegmentationPanel&) = delete;
    SegmentationPanel& operator=(const SegmentationPanel&) = delete;

    /**
     * @brief Get the currently selected segmentation tool
     * @return Current tool
     */
    [[nodiscard]] services::SegmentationTool currentTool() const;

    /**
     * @brief Get current brush size
     * @return Brush size in pixels (1-50)
     */
    [[nodiscard]] int brushSize() const;

    /**
     * @brief Get current brush shape
     * @return Brush shape
     */
    [[nodiscard]] services::BrushShape brushShape() const;

    /**
     * @brief Get current active label ID
     * @return Label ID (1-255)
     */
    [[nodiscard]] uint8_t activeLabel() const;

    /**
     * @brief Get current label color
     * @return Label color
     */
    [[nodiscard]] services::LabelColor labelColor() const;

    /**
     * @brief Set panel enabled state based on image availability
     * @param enabled Enable state
     */
    void setEnabled(bool enabled);

    /**
     * @brief Reset all tools to default state
     */
    void resetToDefaults();

    /**
     * @brief Update enabled state of undo/redo buttons
     * @param canUndo Whether undo is available
     * @param canRedo Whether redo is available
     */
    void setUndoRedoEnabled(bool canUndo, bool canRedo);

signals:
    /**
     * @brief Emitted when segmentation tool changes
     * @param tool New tool
     */
    void toolChanged(services::SegmentationTool tool);

    /**
     * @brief Emitted when brush size changes
     * @param size New brush size
     */
    void brushSizeChanged(int size);

    /**
     * @brief Emitted when brush shape changes
     * @param shape New brush shape
     */
    void brushShapeChanged(services::BrushShape shape);

    /**
     * @brief Emitted when active label changes
     * @param labelId New label ID
     */
    void activeLabelChanged(uint8_t labelId);

    /**
     * @brief Emitted when label color changes
     * @param color New color
     */
    void labelColorChanged(const services::LabelColor& color);

    /**
     * @brief Emitted when clear all is requested
     */
    void clearAllRequested();

    /**
     * @brief Emitted when undo is requested for polygon/smart scissors
     */
    void undoRequested();

    /**
     * @brief Emitted when complete is requested for polygon/smart scissors
     */
    void completeRequested();

    /**
     * @brief Emitted when command stack undo is requested (Ctrl+Z)
     */
    void undoCommandRequested();

    /**
     * @brief Emitted when command stack redo is requested (Ctrl+Y)
     */
    void redoCommandRequested();

    /**
     * @brief Emitted when Hollow operation is requested on current mask
     */
    void hollowRequested();

    /**
     * @brief Emitted when Smoothing operation is requested on current mask
     */
    void smoothRequested();

    /**
     * @brief Emitted when centerline radius override changes
     * @param radiusMm Radius in mm (-1 for auto)
     */
    void centerlineRadiusChanged(double radiusMm);

    /**
     * @brief Emitted when centerline confirm is requested
     */
    void centerlineConfirmRequested();

    /**
     * @brief Emitted when centerline cancel is requested
     */
    void centerlineCancelRequested();

private slots:
    void onToolButtonClicked(int toolId);
    void onBrushSizeChanged(int size);
    void onBrushShapeChanged(int index);
    void onLabelChanged(int index);
    void onColorButtonClicked();
    void onClearAllClicked();
    void onUndoClicked();
    void onCompleteClicked();

private:
    void setupUI();
    void setupConnections();
    void createToolSection();
    void createBrushSection();
    void createLabelSection();
    void createActionSection();
    void updateToolOptions();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

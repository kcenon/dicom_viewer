#pragma once

#include <memory>
#include <QWidget>

#include "services/segmentation/manual_segmentation_controller.hpp"

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

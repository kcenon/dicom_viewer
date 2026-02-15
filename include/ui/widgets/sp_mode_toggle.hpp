#pragma once

#include <memory>
#include <QWidget>

namespace dicom_viewer::ui {

/**
 * @brief Scroll mode for viewer panels
 *
 * Controls whether mouse wheel scrolls through slices (S) or phases (P).
 */
enum class ScrollMode {
    Slice,   ///< Scroll wheel navigates slices (default)
    Phase    ///< Scroll wheel navigates cardiac phases
};

/**
 * @brief Toggle widget for S/P (Slice/Phase) scroll mode
 *
 * Provides two mutually exclusive buttons [S] [P] that switch
 * the scroll wheel behavior between slice navigation and phase
 * navigation in 4D Flow MRI viewers.
 *
 * @trace SRS-FR-048
 */
class SPModeToggle : public QWidget {
    Q_OBJECT

public:
    explicit SPModeToggle(QWidget* parent = nullptr);
    ~SPModeToggle() override;

    // Non-copyable
    SPModeToggle(const SPModeToggle&) = delete;
    SPModeToggle& operator=(const SPModeToggle&) = delete;

    /**
     * @brief Get current scroll mode
     */
    [[nodiscard]] ScrollMode mode() const;

public slots:
    /**
     * @brief Set the scroll mode
     * @param mode Slice or Phase mode
     */
    void setMode(ScrollMode mode);

signals:
    /**
     * @brief Emitted when the user changes the scroll mode
     * @param mode New scroll mode
     */
    void modeChanged(ScrollMode mode);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

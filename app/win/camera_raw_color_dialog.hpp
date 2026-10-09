#pragma once
#include <QWidget>
#include <functional>
#include <nlohmann/json.hpp>

namespace compositor::appwin {

/// The Camera Raw Curve / Color Mixer / Color Grading controls as a plain widget, for the docked tool
/// panel: edits `style` (a Camera Raw object) live and calls `on_change` after each edit so the canvas
/// can preview. The macOS `CameraRawColorControls`.
[[nodiscard]] QWidget* build_camera_raw_color_panel(QWidget* parent, nlohmann::json& style,
                                                    std::function<void()> on_change);

/// The same controls in a dialog, for callers that want a blocking editor. False on Cancel.
[[nodiscard]] bool prompt_camera_raw_color(QWidget* parent, nlohmann::json& adjustment);

}  // namespace compositor::appwin

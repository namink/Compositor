#pragma once
#include <QWidget>
#include <nlohmann/json.hpp>

namespace compositor::appwin {

/// The Hue/Saturation sheet (macOS's `HueSaturationSheet`): a hue/saturation wheel plus a lightness
/// slider. `value` holds the adjustment (`hue` ±180, `saturation` ±100, `lightness` ±100) on the way in
/// and the edited one on the way out. Returns false on Cancel.
[[nodiscard]] bool prompt_hsv(QWidget* parent, nlohmann::json& value);

}  // namespace compositor::appwin

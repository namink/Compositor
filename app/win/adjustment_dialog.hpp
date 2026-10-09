#pragma once
#include <QString>
#include <nlohmann/json.hpp>
#include <vector>

class QWidget;

namespace compositor::appwin {

/// Let the user set an adjustment's or filter's settings. `kind` is the adjustment kind; `value` holds
/// the current settings on the way in and the edited ones on the way out. Returns false on Cancel.
[[nodiscard]] bool prompt_adjustment(QWidget* parent, const QString& kind, const QString& title, nlohmann::json& value);

/// The list of adjustment kinds, in the order the menus present them.
[[nodiscard]] const std::vector<QString>& adjustment_kinds();

/// The list of filter kinds (adjustment kinds plus destructive filters such as Dither).
[[nodiscard]] const std::vector<QString>& filter_kinds();

/// Dither settings: a style, the tone/quantization controls, and Original/light-on-dark switches. `value`
/// holds the current settings on the way in and the edited ones on the way out. False on Cancel.
[[nodiscard]] bool prompt_dither(QWidget* parent, nlohmann::json& value);

}  // namespace compositor::appwin

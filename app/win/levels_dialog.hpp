#pragma once
#include <QWidget>
#include <array>
#include <nlohmann/json.hpp>

namespace compositor::appwin {

/// The Levels sheet (macOS's `LevelsSheet`): the image's histogram with input black/gamma/white
/// controls. `bins` is the 4x256 histogram (RGB, red, green, blue). `value` holds the adjustment on
/// the way in and the edited one on the way out. Returns false on Cancel.
[[nodiscard]] bool prompt_levels(QWidget* parent, const std::array<double, 1024>& bins, nlohmann::json& value);

}  // namespace compositor::appwin

#pragma once
#include <QWidget>
#include <nlohmann/json.hpp>

namespace compositor::appwin {

/// Edit a Curves adjustment's control points on a graph (macOS's `CurvesControls`): a master RGB curve
/// plus one per channel. `value` holds the current adjustment on the way in and the edited one on the
/// way out. Returns false on Cancel.
[[nodiscard]] bool prompt_curves(QWidget* parent, nlohmann::json& value);

}  // namespace compositor::appwin

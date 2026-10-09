#pragma once
#include <QWidget>
#include <nlohmann/json.hpp>

namespace compositor::appwin {

/// Edit a layer's effects (macOS's `EffectsSheet`): stroke, drop shadow, outer/inner glow, inner
/// shadow and color overlay, each with an enable switch, color and its own fields. `effects` holds the
/// current object on the way in and the edited one on the way out (empty when nothing is on).
[[nodiscard]] bool prompt_layer_effects(QWidget* parent, nlohmann::json& effects);

}  // namespace compositor::appwin

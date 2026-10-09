#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace compositor::model {

/// Photoshop's layer blend modes, in its menu order. The manifest spells them out exactly as the
/// macOS app names them (for example "Linear Dodge (Add)"), so the string is the identity.
enum class LayerBlendMode {
    normal,
    darken,
    multiply,
    color_burn,
    linear_burn,
    lighten,
    screen,
    color_dodge,
    linear_dodge,
    overlay,
    soft_light,
    hard_light,
    vivid_light,
    linear_light,
    pin_light,
    hard_mix,
    difference,
    exclusion,
    subtract,
    divide,
    hue,
    saturation,
    color,
    luminosity,
};

[[nodiscard]] std::string_view to_string(LayerBlendMode mode);
[[nodiscard]] bool blend_mode_from_string(std::string_view text, LayerBlendMode& out);

/// Every mode, in menu order (the order Shift-+ / Shift-− steps through).
[[nodiscard]] const std::vector<LayerBlendMode>& all_blend_modes();

}  // namespace compositor::model

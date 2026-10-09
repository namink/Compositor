#include "compositor/model/blend_mode.hpp"

#include <array>
#include <utility>

namespace compositor::model {
namespace {

using Entry = std::pair<LayerBlendMode, std::string_view>;

// The manifest spelling is the identity, so each mode maps to exactly one string and back.
constexpr std::array<Entry, 24> kEntries{{
    {LayerBlendMode::normal, "Normal"},
    {LayerBlendMode::darken, "Darken"},
    {LayerBlendMode::multiply, "Multiply"},
    {LayerBlendMode::color_burn, "Color Burn"},
    {LayerBlendMode::linear_burn, "Linear Burn"},
    {LayerBlendMode::lighten, "Lighten"},
    {LayerBlendMode::screen, "Screen"},
    {LayerBlendMode::color_dodge, "Color Dodge"},
    {LayerBlendMode::linear_dodge, "Linear Dodge (Add)"},
    {LayerBlendMode::overlay, "Overlay"},
    {LayerBlendMode::soft_light, "Soft Light"},
    {LayerBlendMode::hard_light, "Hard Light"},
    {LayerBlendMode::vivid_light, "Vivid Light"},
    {LayerBlendMode::linear_light, "Linear Light"},
    {LayerBlendMode::pin_light, "Pin Light"},
    {LayerBlendMode::hard_mix, "Hard Mix"},
    {LayerBlendMode::difference, "Difference"},
    {LayerBlendMode::exclusion, "Exclusion"},
    {LayerBlendMode::subtract, "Subtract"},
    {LayerBlendMode::divide, "Divide"},
    {LayerBlendMode::hue, "Hue"},
    {LayerBlendMode::saturation, "Saturation"},
    {LayerBlendMode::color, "Color"},
    {LayerBlendMode::luminosity, "Luminosity"},
}};

}  // namespace

std::string_view to_string(LayerBlendMode mode) {
    for (const auto& [candidate, text] : kEntries) {
        if (candidate == mode) {
            return text;
        }
    }
    return "Normal";
}

bool blend_mode_from_string(std::string_view text, LayerBlendMode& out) {
    for (const auto& [mode, name] : kEntries) {
        if (name == text) {
            out = mode;
            return true;
        }
    }
    return false;
}

const std::vector<LayerBlendMode>& all_blend_modes() {
    static const std::vector<LayerBlendMode> kModes = [] {
        std::vector<LayerBlendMode> modes;
        modes.reserve(kEntries.size());
        for (const auto& [mode, name] : kEntries) {
            modes.push_back(mode);
        }
        return modes;
    }();
    return kModes;
}

}  // namespace compositor::model

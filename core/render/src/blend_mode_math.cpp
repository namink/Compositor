#include "compositor/render/blend_mode_math.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace compositor::render {
namespace {

using model::LayerBlendMode;

[[nodiscard]] float clamp01(float value) {
    return std::clamp(value, 0.0F, 1.0F);
}

[[nodiscard]] float color_dodge(float cb, float cs) {
    if (cb <= 0.0F) {
        return 0.0F;
    }
    if (cs >= 1.0F) {
        return 1.0F;
    }
    return std::min(1.0F, cb / (1.0F - cs));
}

[[nodiscard]] float color_burn(float cb, float cs) {
    if (cb >= 1.0F) {
        return 1.0F;
    }
    if (cs <= 0.0F) {
        return 0.0F;
    }
    return 1.0F - std::min(1.0F, (1.0F - cb) / cs);
}

/// The W3C soft light the macOS app reaches for Core Image to get: Core Graphics' own version drifts
/// up to 25 levels with a light blend color.
[[nodiscard]] float soft_light(float cb, float cs) {
    if (cs <= 0.5F) {
        return cb - (1.0F - 2.0F * cs) * cb * (1.0F - cb);
    }
    const float curve = cb <= 0.25F ? ((16.0F * cb - 12.0F) * cb + 4.0F) * cb : std::sqrt(cb);
    return cb + (2.0F * cs - 1.0F) * (curve - cb);
}

[[nodiscard]] float hard_light(float cb, float cs) {
    return cs <= 0.5F ? 2.0F * cs * cb : 1.0F - 2.0F * (1.0F - cs) * (1.0F - cb);
}

[[nodiscard]] float vivid_light(float cb, float cs) {
    return cs <= 0.5F ? color_burn(cb, 2.0F * cs) : color_dodge(cb, 2.0F * (cs - 0.5F));
}

[[nodiscard]] float hard_mix(float cb, float cs) {
    return vivid_light(cb, cs) < 0.5F ? 0.0F : 1.0F;
}

[[nodiscard]] float pin_light(float cb, float cs) {
    return cs <= 0.5F ? std::min(cb, 2.0F * cs) : std::max(cb, 2.0F * cs - 1.0F);
}

[[nodiscard]] float luminance(const RgbF& color) {
    return 0.3F * color.r + 0.59F * color.g + 0.11F * color.b;
}

[[nodiscard]] RgbF clip_color(RgbF color) {
    const float l = luminance(color);
    const float low = std::min({color.r, color.g, color.b});
    const float high = std::max({color.r, color.g, color.b});
    if (low < 0.0F) {
        const float range = l - low;
        color.r = range == 0.0F ? l : l + (color.r - l) * l / range;
        color.g = range == 0.0F ? l : l + (color.g - l) * l / range;
        color.b = range == 0.0F ? l : l + (color.b - l) * l / range;
    }
    if (high > 1.0F) {
        const float range = high - l;
        color.r = range == 0.0F ? l : l + (color.r - l) * (1.0F - l) / range;
        color.g = range == 0.0F ? l : l + (color.g - l) * (1.0F - l) / range;
        color.b = range == 0.0F ? l : l + (color.b - l) * (1.0F - l) / range;
    }
    return color;
}

[[nodiscard]] RgbF set_luminance(RgbF color, float l) {
    const float delta = l - luminance(color);
    color.r += delta;
    color.g += delta;
    color.b += delta;
    return clip_color(color);
}

[[nodiscard]] float saturation(const RgbF& color) {
    return std::max({color.r, color.g, color.b}) - std::min({color.r, color.g, color.b});
}

[[nodiscard]] RgbF set_saturation(RgbF color, float s) {
    std::array<float, 3> value{color.r, color.g, color.b};
    std::array<int, 3> order{0, 1, 2};
    std::sort(order.begin(), order.end(), [&](int a, int b) { return value[a] < value[b]; });
    const float low = value[order[0]];
    const float mid = value[order[1]];
    const float high = value[order[2]];
    std::array<float, 3> result{};
    if (high > low) {
        result[order[0]] = 0.0F;
        result[order[1]] = ((mid - low) * s) / (high - low);
        result[order[2]] = s;
    } else {
        result[order[1]] = 0.0F;
        result[order[2]] = 0.0F;
    }
    return RgbF{result[0], result[1], result[2]};
}

}  // namespace

float blend_channel(LayerBlendMode mode, float cb, float cs) {
    switch (mode) {
    case LayerBlendMode::normal:
        return cs;
    case LayerBlendMode::darken:
        return std::min(cb, cs);
    case LayerBlendMode::multiply:
        return cb * cs;
    case LayerBlendMode::color_burn:
        return color_burn(cb, cs);
    case LayerBlendMode::linear_burn:
        return clamp01(cb + cs - 1.0F);
    case LayerBlendMode::lighten:
        return std::max(cb, cs);
    case LayerBlendMode::screen:
        return cb + cs - cb * cs;
    case LayerBlendMode::color_dodge:
        return color_dodge(cb, cs);
    case LayerBlendMode::linear_dodge:
        return clamp01(cb + cs);
    case LayerBlendMode::overlay:
        return hard_light(cs, cb);
    case LayerBlendMode::soft_light:
        return soft_light(cb, cs);
    case LayerBlendMode::hard_light:
        return hard_light(cb, cs);
    case LayerBlendMode::vivid_light:
        return vivid_light(cb, cs);
    case LayerBlendMode::linear_light:
        return clamp01(cb + 2.0F * cs - 1.0F);
    case LayerBlendMode::pin_light:
        return pin_light(cb, cs);
    case LayerBlendMode::hard_mix:
        return hard_mix(cb, cs);
    case LayerBlendMode::difference:
        return std::abs(cb - cs);
    case LayerBlendMode::exclusion:
        return cb + cs - 2.0F * cb * cs;
    case LayerBlendMode::subtract:
        return clamp01(cb - cs);
    case LayerBlendMode::divide:
        return cs <= 0.0F ? 1.0F : clamp01(cb / cs);
    case LayerBlendMode::hue:
    case LayerBlendMode::saturation:
    case LayerBlendMode::color:
    case LayerBlendMode::luminosity:
        return cs;  // handled by `blend_color`; a per-channel call has no meaningful answer.
    }
    return cs;
}

RgbF blend_color(LayerBlendMode mode, const RgbF& cb, const RgbF& cs) {
    switch (mode) {
    case LayerBlendMode::hue:
        return set_luminance(set_saturation(cs, saturation(cb)), luminance(cb));
    case LayerBlendMode::saturation:
        return set_luminance(set_saturation(cb, saturation(cs)), luminance(cb));
    case LayerBlendMode::color:
        return set_luminance(cs, luminance(cb));
    case LayerBlendMode::luminosity:
        return set_luminance(cb, luminance(cs));
    default:
        return RgbF{blend_channel(mode, cb.r, cs.r), blend_channel(mode, cb.g, cs.g), blend_channel(mode, cb.b, cs.b)};
    }
}

}  // namespace compositor::render

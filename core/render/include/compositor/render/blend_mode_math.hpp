#pragma once
#include "compositor/model/blend_mode.hpp"
#include "compositor/render/rgba_surface.hpp"

namespace compositor::render {

/// The blend functions from the W3C Compositing and Blending spec, which is what the macOS app's
/// Core Image blend modes compute. Colors are unpremultiplied sRGB in [0, 1], and the blend is done
/// in sRGB (not linear light) so Color Burn and Dodge match Photoshop — see the macOS app's
/// `SeparableBlend`, which exists for exactly that reason.
///
/// `cb` is the backdrop channel, `cs` the source channel.
[[nodiscard]] float blend_channel(model::LayerBlendMode mode, float cb, float cs);

/// The blended color for `mode`. For the separable modes this is `blend_channel` on each channel;
/// Hue, Saturation, Color and Luminosity use the non-separable luminosity formulas.
[[nodiscard]] RgbF blend_color(model::LayerBlendMode mode, const RgbF& cb, const RgbF& cs);

}  // namespace compositor::render

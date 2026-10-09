#pragma once
#include <cstddef>
#include <cstdint>
#include <nlohmann/json.hpp>

#include "compositor/render/rgba_surface.hpp"

namespace compositor::render::detail {

/// Camera Raw Curve / Mixer / Grading applied in place to premultiplied pixels, through the shared
/// `adjust_camera_raw_curve_color` kernel. Does nothing when the adjustment has none of the three.
void apply_camera_raw_curve_color(const nlohmann::json& adjustment, std::uint8_t* pixels, int width, int height,
                                  std::size_t stride);

/// Hue/Saturation applied in place through a 33-point color cube, as the macOS app does. Always
/// succeeds; identity settings leave the pixels untouched.
void apply_hsv(const nlohmann::json& adjustment, RgbaSurface& surface);

/// Curves applied in place through a per-channel lookup table, as the macOS app does.
void apply_curves(const nlohmann::json& adjustment, RgbaSurface& surface);

}  // namespace compositor::render::detail

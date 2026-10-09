#pragma once
#include <string_view>

#include "compositor/render/rgba_surface.hpp"

namespace compositor::render {

/// A shape a Shape layer draws, matching the macOS app's `ShapeKind`.
enum class ShapeKind { rectangle, ellipse, line };

[[nodiscard]] std::string_view shape_kind_name(ShapeKind kind);

/// Everything needed to draw a shape at any size, matching the macOS app's `LayerShapeStyle`. A line
/// keeps its two ends as fractions of the layer's box (0–1) so a scaled line still runs between the
/// same two places.
struct ShapeStyle {
    ShapeKind kind = ShapeKind::rectangle;
    float red = 0.0F;
    float green = 0.0F;
    float blue = 0.0F;
    /// Rectangle corner radius in document pixels (ignored by ellipses and lines).
    double corner_radius = 0.0;
    /// A line's stroke thickness in document pixels.
    double line_width = 0.0;
    bool has_ends = false;
    double start_x = 0.0;
    double start_y = 0.0;
    double end_x = 1.0;
    double end_y = 1.0;
};

/// Draw the shape filling a `width`×`height` premultiplied sRGB surface, anti-aliased. A line is
/// stroked with round caps; the other kinds are filled.
[[nodiscard]] RgbaSurface draw_shape(const ShapeStyle& style, int width, int height);

}  // namespace compositor::render

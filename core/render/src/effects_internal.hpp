#pragma once
#include <cstdint>
#include <vector>

#include "compositor/render/rgba_surface.hpp"

namespace compositor::render::detail {

/// A single-channel coverage map (white reveals) as floats in [0, 1], row-major, top row first.
using Mask = std::vector<float>;

/// Gaussian blur of a coverage map with edges clamped, matching the macOS app's
/// `clampedToExtent().applyingGaussianBlur(...)` for effect coverage.
[[nodiscard]] Mask blur_mask(const Mask& mask, int width, int height, double sigma);

/// The largest (or smallest) value within `reach` each way, by two monotonic-deque passes, as the
/// macOS app's `extremes` does for strokes (a square reach).
[[nodiscard]] Mask extremes(const Mask& source, int width, int height, int reach, bool smallest);

/// Paint `color` at `opacity * coverage` over a premultiplied float RGBA surface (source-over).
void fill_over(std::vector<float>& destination, int width, int height, double red, double green, double blue,
               double opacity, const Mask& coverage);

/// Draw a premultiplied float RGBA image over another, source-over, at (dx, dy).
void draw_over(std::vector<float>& destination, int destination_width, int destination_height,
               const std::vector<float>& source, int source_width, int source_height, int dx, int dy);

/// The alpha of an image placed at (offset_x, offset_y) in a larger coverage map, zero elsewhere.
[[nodiscard]] Mask shape_coverage(const std::uint8_t* premultiplied, int image_width, int image_height, int map_width,
                                  int map_height, int offset_x, int offset_y);

/// A premultiplied float RGBA surface from raw bytes.
[[nodiscard]] std::vector<float> to_float_rgba(const std::uint8_t* bytes, std::size_t pixels);

/// Raw premultiplied bytes from a float RGBA surface.
[[nodiscard]] RgbaSurface from_float_rgba(const std::vector<float>& values, int width, int height);

}  // namespace compositor::render::detail

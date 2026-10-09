#pragma once
#include <cstdint>
#include <span>
#include <vector>

#include "compositor/model/layer_transform.hpp"
#include "compositor/render/rgba_surface.hpp"

namespace compositor::render {

/// An axis-aligned rectangle in document pixels, top-left origin, y down.
struct DocRect {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;

    [[nodiscard]] bool empty() const { return width <= 0.0 || height <= 0.0; }
    [[nodiscard]] bool contains(double px, double py) const {
        return px >= x && px < x + width && py >= y && py < y + height;
    }
};

struct Point {
    double x = 0.0;
    double y = 0.0;
};

/// A document-sized selection: 8-bit coverage, 255 selected, 0 not. Every selection tool — rectangle,
/// lasso, magic wand — produces one of these, so the operations below work the same for all of them.
struct Selection {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> coverage;
    int min_x = 0;
    int min_y = 0;
    int max_x = -1;
    int max_y = -1;

    [[nodiscard]] bool empty() const { return coverage.empty() || max_x < min_x || max_y < min_y; }
    [[nodiscard]] std::uint8_t at(int x, int y) const {
        if (x < 0 || y < 0 || x >= width || y >= height) {
            return 0;
        }
        return coverage[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
    }
};

enum class CombineMode { replace, add, subtract, intersect };

[[nodiscard]] Selection from_rect(const DocRect& rect, int document_width, int document_height);

/// An ellipse inscribed in `bounds` (document pixels).
[[nodiscard]] Selection from_ellipse(const DocRect& bounds, int document_width, int document_height);

/// Rasterize a closed polygon (document pixels) with the even-odd rule.
[[nodiscard]] Selection from_polygon(std::span<const Point> points, int document_width, int document_height);

/// Magic wand on a layer's own pixels: match the seed within `tolerance`, 4-connected or everywhere,
/// and return the result in document space.
[[nodiscard]] Selection magic_wand(const RgbaSurface& sample, const model::LayerTransform& transform,
                                   int document_width, int document_height, double click_x, double click_y,
                                   int tolerance, bool contiguous);

/// Select > Color Range: a document-sized selection of the pixels of `image` near any of the `include`
/// colors (each three bytes, straight sRGB) and not near any `exclude`, within `fuzziness`. The macOS
/// app's `color_range_mask` kernel. `invert` swaps selected for unselected.
[[nodiscard]] Selection color_range(const RgbaSurface& image, const std::vector<std::uint8_t>& include,
                                    const std::vector<std::uint8_t>& exclude, int fuzziness, bool invert);

[[nodiscard]] Selection combine(const Selection& base, const Selection& other, CombineMode mode);
[[nodiscard]] Selection invert(const Selection& selection);
[[nodiscard]] Selection feather(const Selection& selection, double radius);
[[nodiscard]] Selection expand(const Selection& selection, int radius);
[[nodiscard]] Selection contract(const Selection& selection, int radius);

/// The selection's outline as closed loops of document-space points (from the C `wand_trace` kernel),
/// for drawing the "marching ants". Empty when the outline is too detailed to trace cheaply.
[[nodiscard]] std::vector<std::vector<Point>> outline(const Selection& selection);

/// Clear or fill the layer's pixels, weighted by their document position's coverage in `selection`.
void clear_in_layer(RgbaSurface& layer, const model::LayerTransform& transform, const Selection& selection);
void fill_in_layer(RgbaSurface& layer, const model::LayerTransform& transform, const Selection& selection, double red,
                   double green, double blue, double opacity);

}  // namespace compositor::render

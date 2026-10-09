#pragma once
#include <cstdint>
#include <vector>

#include "compositor/model/layer_transform.hpp"
#include "compositor/render/rgba_surface.hpp"
#include "compositor/render/selection.hpp"

namespace compositor::render {

/// One brush stamped at a point, in the layer's own pixel coordinates.
struct BrushDab {
    /// Center, in layer pixels (pixel (x, y) covers [x, x+1) x [y, y+1)).
    double x = 0.0;
    double y = 0.0;
    /// Radius in pixels.
    double radius = 10.0;
    /// 0 is a fully soft edge, 1 a hard one.
    double hardness = 0.7;
    /// Full-strength coverage at the center, 0–1.
    double opacity = 1.0;
    /// Straight sRGB color.
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
    /// Erase lowers coverage instead of adding color.
    bool erase = false;
};

/// Stamp a dab onto a premultiplied surface. A soft round brush: full strength out to
/// `hardness * radius`, falling to nothing at the radius.
void stamp_dab(RgbaSurface& surface, const BrushDab& dab);

/// Stamp dabs along the segment from (x0, y0) to (x1, y1), spaced so a drag paints a continuous
/// stroke rather than a string of separate dots.
void stroke_segment(RgbaSurface& surface, double x0, double y0, double x1, double y1, const BrushDab& dab);

/// A gradient from color A to color B along the document-space line `start`..`end` (linear) or from
/// `start` outward to `end` (radial). `clip`, when given, restricts it to a selection. Straight sRGB,
/// lerped in premultiplied form and composited over the layer.
void draw_gradient(RgbaSurface& layer, const model::LayerTransform& transform, const Selection* clip, double a_red,
                   double a_green, double a_blue, double a_alpha, double b_red, double b_green, double b_blue,
                   double b_alpha, double start_x, double start_y, double end_x, double end_y, bool radial);

/// Spot healing: rebuild the pixels where `coverage` (0–255, the surface's size) is set, from their
/// surroundings (the macOS app's `spot_heal` C kernel). `mode` 0 content-aware, 1 create texture,
/// 2 proximity match.
void spot_heal(RgbaSurface& surface, const std::vector<std::uint8_t>& coverage, double opacity, int mode,
               std::uint32_t seed);

/// Content-Aware Fill: synthesize the selected pixels of `surface` from their unselected, opaque
/// surroundings (the macOS app's `content_fill` C kernel). `selection` is document-sized and `surface`
/// is the same size. False when there is not enough surrounding image to sample from.
bool content_fill(RgbaSurface& surface, const Selection& selection);

/// Clone stamp: copy from `source` to `dest` (both layer pixels) with a soft round brush. The source
/// is offset from the destination by the same delta across a whole stroke.
void clone_dab(RgbaSurface& surface, double source_x, double source_y, double dest_x, double dest_y, double radius,
               double hardness, double opacity);

/// Blur brush: soften the pixels under a soft round brush by `sigma`, blended in by `strength`.
void blur_dab(RgbaSurface& surface, double center_x, double center_y, double radius, double hardness, double sigma,
              double strength);

}  // namespace compositor::render

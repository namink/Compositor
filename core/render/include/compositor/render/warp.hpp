#pragma once
#include <vector>

#include "compositor/render/rgba_surface.hpp"

namespace compositor::render {

/// The Blur tool's modes, matching the macOS app's `BlurToolMode`. Smudge and Liquify push the
/// layer's pixels around under the brush; Blur is handled with the painting code.
enum class WarpMode { liquify, smudge };

/// A Smudge or Liquify stroke, ported from the macOS app's `WarpStroke`. It works on the layer's own
/// pixels dab by dab. Smudge carries the color under the brush along the drag; Liquify pushes pixels
/// forward, most at the brush center, fading to none at its rim.
class WarpStroke {
public:
    WarpStroke(RgbaSurface& surface, WarpMode mode, double diameter, double hardness, double strength);

    /// Continues the stroke to (x, y) in layer pixels, dabbing along the way.
    void append(double x, double y);

    [[nodiscard]] bool has_points() const { return !points_.empty(); }

private:
    [[nodiscard]] float weight(float u) const;
    void pick_up(double x, double y);
    void smudge_at(double x, double y);
    void push(double ax, double ay, double bx, double by);

    RgbaSurface& surface_;
    WarpMode mode_;
    double diameter_;
    double hardness_;
    double strength_;
    int radius_;
    bool started_ = false;
    double last_x_ = 0.0;
    double last_y_ = 0.0;
    std::vector<float> carried_;
    std::vector<float> scratch_;
    std::vector<std::pair<double, double>> points_;
};

/// A quadrilateral in document pixels, corners in the order top-left, top-right, bottom-right,
/// bottom-left — what a Distort drag moves.
struct Quad {
    double x[4] = {0.0, 0.0, 0.0, 0.0};
    double y[4] = {0.0, 0.0, 0.0, 0.0};
};

/// Resample `image` so its corners land on `corners` (via a perspective homography), returning the
/// warped pixels over the shape's whole-pixel bounds and reporting those bounds in `origin_x`/
/// `origin_y`. `flip_x`/`flip_y` send the image to the opposite corners, as a flipped layer shows.
/// False when the quad has no area. Ported from the macOS app's `DistortWarp`.
[[nodiscard]] bool warp_perspective(const RgbaSurface& image, const Quad& corners, bool flip_x, bool flip_y,
                                    int& origin_x, int& origin_y, RgbaSurface& out);

}  // namespace compositor::render

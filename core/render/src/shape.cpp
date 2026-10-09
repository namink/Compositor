#include "compositor/render/shape.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace compositor::render {
namespace {

constexpr int kSamples = 4;  // 4×4 supersampling per pixel

/// True when a point (in pixel space, [0, width]×[0, height]) is inside the shape.
[[nodiscard]] bool inside_rectangle(double px, double py, double width, double height, double radius) {
    const double clamped = std::min({std::max(0.0, radius), width / 2.0, height / 2.0});
    if (clamped <= 0.0) {
        return px >= 0.0 && py >= 0.0 && px <= width && py <= height;
    }
    const double inner_left = clamped;
    const double inner_right = width - clamped;
    const double inner_top = clamped;
    const double inner_bottom = height - clamped;
    if (px >= inner_left && px <= inner_right) {
        return py >= 0.0 && py <= height;
    }
    if (py >= inner_top && py <= inner_bottom) {
        return px >= 0.0 && px <= width;
    }
    const double cx = px < inner_left ? inner_left : inner_right;
    const double cy = py < inner_top ? inner_top : inner_bottom;
    const double dx = px - cx;
    const double dy = py - cy;
    return dx * dx + dy * dy <= clamped * clamped;
}

[[nodiscard]] bool inside_ellipse(double px, double py, double width, double height) {
    const double rx = width / 2.0;
    const double ry = height / 2.0;
    if (rx <= 0.0 || ry <= 0.0) {
        return false;
    }
    const double dx = (px - rx) / rx;
    const double dy = (py - ry) / ry;
    return dx * dx + dy * dy <= 1.0;
}

[[nodiscard]] double distance_to_segment(double px, double py, double ax, double ay, double bx, double by) {
    const double vx = bx - ax;
    const double vy = by - ay;
    const double length_sq = vx * vx + vy * vy;
    double t = 0.0;
    if (length_sq > 0.0) {
        t = std::clamp(((px - ax) * vx + (py - ay) * vy) / length_sq, 0.0, 1.0);
    }
    const double dx = px - (ax + t * vx);
    const double dy = py - (ay + t * vy);
    return std::sqrt(dx * dx + dy * dy);
}

[[nodiscard]] bool inside_line(double px, double py, double width, double height, const ShapeStyle& style) {
    const double thickness = std::max(1.0, style.line_width);
    double ax = 0.0;
    double ay = 0.0;
    double bx = width;
    double by = height;
    if (style.has_ends) {
        ax = style.start_x * width;
        ay = style.start_y * height;
        bx = style.end_x * width;
        by = style.end_y * height;
    } else {
        const double inset_x = std::min(thickness, width) / 2.0;
        const double inset_y = std::min(thickness, height) / 2.0;
        ax = inset_x;
        ay = inset_y;
        bx = width - inset_x;
        by = height - inset_y;
    }
    return distance_to_segment(px, py, ax, ay, bx, by) <= thickness / 2.0;
}

}  // namespace

std::string_view shape_kind_name(ShapeKind kind) {
    switch (kind) {
    case ShapeKind::ellipse:
        return "Ellipse";
    case ShapeKind::line:
        return "Line";
    case ShapeKind::rectangle:
    default:
        return "Rectangle";
    }
}

RgbaSurface draw_shape(const ShapeStyle& style, int width, int height) {
    RgbaSurface surface(width, height);
    if (surface.empty()) {
        return surface;
    }
    const double step = 1.0 / static_cast<double>(kSamples);
    const int total = kSamples * kSamples;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int hits = 0;
            for (int sy = 0; sy < kSamples; ++sy) {
                for (int sx = 0; sx < kSamples; ++sx) {
                    const double px = static_cast<double>(x) + (static_cast<double>(sx) + 0.5) * step;
                    const double py = static_cast<double>(y) + (static_cast<double>(sy) + 0.5) * step;
                    bool inside = false;
                    switch (style.kind) {
                    case ShapeKind::ellipse:
                        inside = inside_ellipse(px, py, width, height);
                        break;
                    case ShapeKind::line:
                        inside = inside_line(px, py, width, height, style);
                        break;
                    case ShapeKind::rectangle:
                    default:
                        inside = inside_rectangle(px, py, width, height, style.corner_radius);
                        break;
                    }
                    if (inside) {
                        ++hits;
                    }
                }
            }
            if (hits == 0) {
                continue;
            }
            const float coverage = static_cast<float>(hits) / static_cast<float>(total);
            RgbF color{style.red, style.green, style.blue};
            std::uint8_t r = 0;
            std::uint8_t g = 0;
            std::uint8_t b = 0;
            std::uint8_t a = 0;
            premultiply(color, coverage, r, g, b, a);
            surface.set(x, y, r, g, b, a);
        }
    }
    return surface;
}

}  // namespace compositor::render

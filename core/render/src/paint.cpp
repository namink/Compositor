#include "compositor/render/paint.hpp"

#include <algorithm>
#include <cmath>

extern "C" {
#include "HealPixels.h"
}

#include "compositor/render/blur.hpp"
#include "compositor/render/placement.hpp"

namespace compositor::render {
namespace {

[[nodiscard]] float coverage_at(double distance, double radius, double hardness) {
    if (radius <= 0.0 || distance >= radius) {
        return 0.0F;
    }
    const double hard = std::clamp(hardness, 0.0, 0.999);
    if (distance <= hard * radius) {
        return 1.0F;
    }
    // Smoothstep from the hard core out to the radius.
    const double t = std::clamp((radius - distance) / (radius * (1.0 - hard)), 0.0, 1.0);
    return static_cast<float>(t * t * (3.0 - 2.0 * t));
}

}  // namespace

void stamp_dab(RgbaSurface& surface, const BrushDab& dab) {
    if (surface.empty() || dab.radius <= 0.0 || dab.opacity <= 0.0) {
        return;
    }
    const int min_x = std::max(0, static_cast<int>(std::floor(dab.x - dab.radius)));
    const int max_x = std::min(surface.width() - 1, static_cast<int>(std::ceil(dab.x + dab.radius)));
    const int min_y = std::max(0, static_cast<int>(std::floor(dab.y - dab.radius)));
    const int max_y = std::min(surface.height() - 1, static_cast<int>(std::ceil(dab.y + dab.radius)));
    std::uint8_t* pixels = surface.data();
    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            const double dx = (static_cast<double>(x) + 0.5) - dab.x;
            const double dy = (static_cast<double>(y) + 0.5) - dab.y;
            const float coverage = coverage_at(std::sqrt(dx * dx + dy * dy), dab.radius, dab.hardness);
            const float alpha = static_cast<float>(dab.opacity) * coverage;
            if (alpha <= 0.0F) {
                continue;
            }
            const std::size_t at = surface.offset(x, y);
            const float keep = 1.0F - alpha;
            if (dab.erase) {
                for (int c = 0; c < 4; ++c) {
                    pixels[at + static_cast<std::size_t>(c)] =
                        static_cast<std::uint8_t>(std::lround(pixels[at + static_cast<std::size_t>(c)] * keep));
                }
                continue;
            }
            const float src_r = static_cast<float>(dab.red) * alpha;
            const float src_g = static_cast<float>(dab.green) * alpha;
            const float src_b = static_cast<float>(dab.blue) * alpha;
            pixels[at] = static_cast<std::uint8_t>(
                std::lround(std::clamp(src_r + static_cast<float>(pixels[at]) / 255.0F * keep, 0.0F, 1.0F) * 255.0F));
            pixels[at + 1] = static_cast<std::uint8_t>(std::lround(
                std::clamp(src_g + static_cast<float>(pixels[at + 1]) / 255.0F * keep, 0.0F, 1.0F) * 255.0F));
            pixels[at + 2] = static_cast<std::uint8_t>(std::lround(
                std::clamp(src_b + static_cast<float>(pixels[at + 2]) / 255.0F * keep, 0.0F, 1.0F) * 255.0F));
            pixels[at + 3] = static_cast<std::uint8_t>(std::lround(
                std::clamp(alpha + static_cast<float>(pixels[at + 3]) / 255.0F * keep, 0.0F, 1.0F) * 255.0F));
        }
    }
}

void draw_gradient(RgbaSurface& layer, const model::LayerTransform& transform, const Selection* clip, double a_red,
                   double a_green, double a_blue, double a_alpha, double b_red, double b_green, double b_blue,
                   double b_alpha, double start_x, double start_y, double end_x, double end_y, bool radial) {
    if (layer.empty()) {
        return;
    }
    const double axis_x = end_x - start_x;
    const double axis_y = end_y - start_y;
    const double length_squared = axis_x * axis_x + axis_y * axis_y;
    const double radius = std::sqrt(length_squared);
    if (length_squared <= 0.0) {
        return;
    }
    std::uint8_t* pixels = layer.data();
    for (int y = 0; y < layer.height(); ++y) {
        for (int x = 0; x < layer.width(); ++x) {
            double doc_x = 0.0;
            double doc_y = 0.0;
            layer_pixel_to_document(transform, layer.width(), layer.height(), x + 0.5, y + 0.5, doc_x, doc_y);
            if (clip != nullptr &&
                clip->at(static_cast<int>(std::floor(doc_x)), static_cast<int>(std::floor(doc_y))) == 0) {
                continue;
            }
            double t = 0.0;
            if (radial) {
                t = std::hypot(doc_x - start_x, doc_y - start_y) / radius;
            } else {
                t = ((doc_x - start_x) * axis_x + (doc_y - start_y) * axis_y) / length_squared;
            }
            t = std::clamp(t, 0.0, 1.0);
            const double red = a_red + (b_red - a_red) * t;
            const double green = a_green + (b_green - a_green) * t;
            const double blue = a_blue + (b_blue - a_blue) * t;
            const float alpha = static_cast<float>(a_alpha + (b_alpha - a_alpha) * t);
            if (alpha <= 0.0F) {
                continue;
            }
            const std::size_t at = layer.offset(x, y);
            const float keep = 1.0F - alpha;
            const float src[3] = {static_cast<float>(red) * alpha, static_cast<float>(green) * alpha,
                                  static_cast<float>(blue) * alpha};
            for (int c = 0; c < 3; ++c) {
                const float value =
                    src[c] + static_cast<float>(pixels[at + static_cast<std::size_t>(c)]) / 255.0F * keep;
                pixels[at + static_cast<std::size_t>(c)] =
                    static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
            }
            const float out_alpha = alpha + static_cast<float>(pixels[at + 3]) / 255.0F * keep;
            pixels[at + 3] = static_cast<std::uint8_t>(std::lround(std::clamp(out_alpha, 0.0F, 1.0F) * 255.0F));
        }
    }
}

void clone_dab(RgbaSurface& surface, double source_x, double source_y, double dest_x, double dest_y, double radius,
               double hardness, double opacity) {
    if (surface.empty() || radius <= 0.0 || opacity <= 0.0) {
        return;
    }
    const int min_x = std::max(0, static_cast<int>(std::floor(dest_x - radius)));
    const int max_x = std::min(surface.width() - 1, static_cast<int>(std::ceil(dest_x + radius)));
    const int min_y = std::max(0, static_cast<int>(std::floor(dest_y - radius)));
    const int max_y = std::min(surface.height() - 1, static_cast<int>(std::ceil(dest_y + radius)));
    const std::uint8_t* source = surface.data();
    std::uint8_t* pixels = surface.data();
    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            const double dx = (x + 0.5) - dest_x;
            const double dy = (y + 0.5) - dest_y;
            const float coverage = coverage_at(std::sqrt(dx * dx + dy * dy), radius, hardness);
            const float alpha = static_cast<float>(opacity) * coverage;
            if (alpha <= 0.0F) {
                continue;
            }
            const int sx = static_cast<int>(std::floor(source_x + dx));
            const int sy = static_cast<int>(std::floor(source_y + dy));
            if (sx < 0 || sy < 0 || sx >= surface.width() || sy >= surface.height()) {
                continue;
            }
            const std::size_t src_at = surface.offset(sx, sy);
            const std::size_t dst_at = surface.offset(x, y);
            const float keep = 1.0F - alpha;
            for (int c = 0; c < 3; ++c) {
                const float value = static_cast<float>(source[src_at + static_cast<std::size_t>(c)]) / 255.0F * alpha +
                                    static_cast<float>(pixels[dst_at + static_cast<std::size_t>(c)]) / 255.0F * keep;
                pixels[dst_at + static_cast<std::size_t>(c)] =
                    static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
            }
            const float out_alpha = static_cast<float>(source[src_at + 3U]) / 255.0F * alpha +
                                    static_cast<float>(pixels[dst_at + 3U]) / 255.0F * keep;
            pixels[dst_at + 3U] = static_cast<std::uint8_t>(std::lround(std::clamp(out_alpha, 0.0F, 1.0F) * 255.0F));
        }
    }
}

void blur_dab(RgbaSurface& surface, double center_x, double center_y, double radius, double hardness, double sigma,
              double strength) {
    if (surface.empty() || radius <= 0.0 || sigma <= 0.0 || strength <= 0.0) {
        return;
    }
    const int margin = static_cast<int>(std::ceil(sigma * 3.0));
    const int x0 = std::max(0, static_cast<int>(std::floor(center_x - radius)) - margin);
    const int x1 = std::min(surface.width() - 1, static_cast<int>(std::ceil(center_x + radius)) + margin);
    const int y0 = std::max(0, static_cast<int>(std::floor(center_y - radius)) - margin);
    const int y1 = std::min(surface.height() - 1, static_cast<int>(std::ceil(center_y + radius)) + margin);
    if (x1 < x0 || y1 < y0) {
        return;
    }
    RgbaSurface region(x1 - x0 + 1, y1 - y0 + 1);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const std::uint8_t* src = surface.data() + surface.offset(x, y);
            region.set(x - x0, y - y0, src[0], src[1], src[2], src[3]);
        }
    }
    const RgbaSurface blurred = gaussian_blur(region, sigma);
    std::uint8_t* pixels = surface.data();
    const std::uint8_t* soft = blurred.data();
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const double distance = std::hypot(x + 0.5 - center_x, y + 0.5 - center_y);
            const float coverage = coverage_at(distance, radius, hardness);
            const float alpha = static_cast<float>(strength) * coverage;
            if (alpha <= 0.0F) {
                continue;
            }
            const std::size_t at = surface.offset(x, y);
            const std::size_t soft_at = blurred.offset(x - x0, y - y0);
            const float keep = 1.0F - alpha;
            for (int c = 0; c < 4; ++c) {
                const float value = static_cast<float>(soft[soft_at + static_cast<std::size_t>(c)]) * alpha +
                                    static_cast<float>(pixels[at + static_cast<std::size_t>(c)]) * keep;
                pixels[at + static_cast<std::size_t>(c)] =
                    static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, 255.0F)));
            }
        }
    }
}

void spot_heal(RgbaSurface& surface, const std::vector<std::uint8_t>& coverage, double opacity, int mode,
               std::uint32_t seed) {
    const std::size_t expected = static_cast<std::size_t>(surface.width()) * static_cast<std::size_t>(surface.height());
    if (surface.empty() || coverage.size() < expected) {
        return;
    }
    ::spot_heal(surface.data(), coverage.data(), static_cast<std::size_t>(surface.width()),
                static_cast<std::size_t>(surface.height()), static_cast<std::size_t>(surface.width()) * 4U,
                static_cast<float>(opacity), mode, seed);
}

void stroke_segment(RgbaSurface& surface, double x0, double y0, double x1, double y1, const BrushDab& dab) {
    const double distance = std::hypot(x1 - x0, y1 - y0);
    const double spacing = std::max(1.0, dab.radius * 0.25);
    const int steps = std::max(1, static_cast<int>(std::ceil(distance / spacing)));
    for (int i = 0; i <= steps; ++i) {
        const double t = static_cast<double>(i) / steps;
        BrushDab placed = dab;
        placed.x = x0 + (x1 - x0) * t;
        placed.y = y0 + (y1 - y0) * t;
        stamp_dab(surface, placed);
    }
}

}  // namespace compositor::render

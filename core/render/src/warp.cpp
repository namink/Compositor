#include "compositor/render/warp.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

// Ported from the macOS app's `WarpStroke` (SmudgeLiquify.swift), CPU path. The carried and scratch
// buffers are floats of premultiplied RGBA, as there.

namespace compositor::render {

WarpStroke::WarpStroke(RgbaSurface& surface, WarpMode mode, double diameter, double hardness, double strength)
    : surface_(surface),
      mode_(mode),
      diameter_(std::max(2.0, diameter)),
      hardness_(std::clamp(hardness, 0.0, 0.98)),
      strength_(std::clamp(strength, 0.01, 1.0)) {
    radius_ = static_cast<int>(std::ceil(diameter_ / 2.0));
}

float WarpStroke::weight(float u) const {
    if (u >= 1.0F) {
        return 0.0F;
    }
    const float h = static_cast<float>(hardness_);
    if (u <= h) {
        return 1.0F;
    }
    const float t = (1.0F - u) / (1.0F - h);
    return t * t * (3.0F - 2.0F * t);
}

void WarpStroke::append(double x, double y) {
    if (!started_) {
        started_ = true;
        last_x_ = x;
        last_y_ = y;
        if (mode_ == WarpMode::smudge) {
            pick_up(x, y);
        }
        return;
    }
    const double distance = std::hypot(x - last_x_, y - last_y_);
    const double spacing = std::max(1.0, diameter_ * (mode_ == WarpMode::smudge ? 0.005 : 0.025));
    if (distance < spacing) {
        return;
    }
    const int steps = static_cast<int>(std::ceil(distance / spacing));
    double previous_x = last_x_;
    double previous_y = last_y_;
    for (int step = 1; step <= steps; ++step) {
        const double t = static_cast<double>(step) / static_cast<double>(steps);
        const double next_x = last_x_ + (x - last_x_) * t;
        const double next_y = last_y_ + (y - last_y_) * t;
        if (mode_ == WarpMode::smudge) {
            smudge_at(next_x, next_y);
        } else {
            push(previous_x, previous_y, next_x, next_y);
        }
        points_.emplace_back(next_x, next_y);
        previous_x = next_x;
        previous_y = next_y;
    }
    last_x_ = x;
    last_y_ = y;
}

void WarpStroke::pick_up(double center_x, double center_y) {
    const int r = radius_;
    const int side = 2 * r + 1;
    carried_.assign(static_cast<std::size_t>(side) * static_cast<std::size_t>(side) * 4U, 0.0F);
    const int cx = static_cast<int>(std::lround(center_x));
    const int cy = static_cast<int>(std::lround(center_y));
    std::uint8_t* pixels = surface_.data();
    for (int dy = -r; dy <= r; ++dy) {
        const int y = cy + dy;
        if (y < 0 || y >= surface_.height()) {
            continue;
        }
        for (int dx = -r; dx <= r; ++dx) {
            const int x = cx + dx;
            if (x < 0 || x >= surface_.width()) {
                continue;
            }
            const std::size_t p = surface_.offset(x, y);
            const std::size_t c =
                (static_cast<std::size_t>(dy + r) * static_cast<std::size_t>(side) + static_cast<std::size_t>(dx + r)) *
                4U;
            for (int k = 0; k < 4; ++k) {
                carried_[c + static_cast<std::size_t>(k)] = static_cast<float>(pixels[p + static_cast<std::size_t>(k)]);
            }
        }
    }
}

void WarpStroke::smudge_at(double center_x, double center_y) {
    const int r = radius_;
    const int side = 2 * r + 1;
    const int cx = static_cast<int>(std::lround(center_x));
    const int cy = static_cast<int>(std::lround(center_y));
    const float keep = static_cast<float>(strength_);
    const float inv_r = 1.0F / static_cast<float>(diameter_ / 2.0);
    std::uint8_t* pixels = surface_.data();
    for (int dy = -r; dy <= r; ++dy) {
        const int y = cy + dy;
        if (y < 0 || y >= surface_.height()) {
            continue;
        }
        for (int dx = -r; dx <= r; ++dx) {
            const int x = cx + dx;
            if (x < 0 || x >= surface_.width()) {
                continue;
            }
            const float w = weight(std::sqrt(static_cast<float>(dx * dx + dy * dy)) * inv_r);
            if (w <= 0.0F) {
                continue;
            }
            const std::size_t p = surface_.offset(x, y);
            const std::size_t c =
                (static_cast<std::size_t>(dy + r) * static_cast<std::size_t>(side) + static_cast<std::size_t>(dx + r)) *
                4U;
            for (int k = 0; k < 4; ++k) {
                const float under = static_cast<float>(pixels[p + static_cast<std::size_t>(k)]);
                const float painted = under + (carried_[c + static_cast<std::size_t>(k)] - under) * w * keep;
                pixels[p + static_cast<std::size_t>(k)] =
                    static_cast<std::uint8_t>(std::clamp(std::lround(painted), 0L, 255L));
                carried_[c + static_cast<std::size_t>(k)] = painted;
            }
        }
    }
}

void WarpStroke::push(double ax, double ay, double bx, double by) {
    const int r = radius_;
    const float move_x = static_cast<float>(bx - ax) * static_cast<float>(strength_);
    const float move_y = static_cast<float>(by - ay) * static_cast<float>(strength_);
    const int margin = static_cast<int>(std::ceil(std::max(std::abs(move_x), std::abs(move_y)))) + 2;
    const int cx = static_cast<int>(std::lround(bx));
    const int cy = static_cast<int>(std::lround(by));
    const int x0 = std::max(0, cx - r - margin);
    const int x1 = std::min(surface_.width() - 1, cx + r + margin);
    const int y0 = std::max(0, cy - r - margin);
    const int y1 = std::min(surface_.height() - 1, cy + r + margin);
    if (x0 > x1 || y0 > y1) {
        return;
    }
    const int cw = x1 - x0 + 1;
    const int ch = y1 - y0 + 1;
    if (scratch_.size() < static_cast<std::size_t>(cw) * static_cast<std::size_t>(ch) * 4U) {
        scratch_.assign(static_cast<std::size_t>(cw) * static_cast<std::size_t>(ch) * 4U, 0.0F);
    }
    std::uint8_t* pixels = surface_.data();
    for (int y = 0; y < ch; ++y) {
        for (int x = 0; x < cw; ++x) {
            const std::size_t p = surface_.offset(x + x0, y + y0);
            const std::size_t s =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(cw) + static_cast<std::size_t>(x)) * 4U;
            for (int k = 0; k < 4; ++k) {
                scratch_[s + static_cast<std::size_t>(k)] = static_cast<float>(pixels[p + static_cast<std::size_t>(k)]);
            }
        }
    }
    const float inv_r = 1.0F / static_cast<float>(diameter_ / 2.0);
    for (int dy = -r; dy <= r; ++dy) {
        const int y = cy + dy;
        if (y < y0 || y > y1) {
            continue;
        }
        for (int dx = -r; dx <= r; ++dx) {
            const int x = cx + dx;
            if (x < x0 || x > x1) {
                continue;
            }
            const float w = weight(std::sqrt(static_cast<float>(dx * dx + dy * dy)) * inv_r);
            if (w <= 0.0F) {
                continue;
            }
            const float sx = std::clamp(static_cast<float>(x - x0) - move_x * w, 0.0F, static_cast<float>(cw - 1));
            const float sy = std::clamp(static_cast<float>(y - y0) - move_y * w, 0.0F, static_cast<float>(ch - 1));
            const int ix = std::min(cw - 2, static_cast<int>(sx));
            const int iy = std::min(ch - 2, static_cast<int>(sy));
            if (ix < 0 || iy < 0) {
                continue;
            }
            const float fx = sx - static_cast<float>(ix);
            const float fy = sy - static_cast<float>(iy);
            const std::size_t s00 =
                (static_cast<std::size_t>(iy) * static_cast<std::size_t>(cw) + static_cast<std::size_t>(ix)) * 4U;
            const std::size_t s10 = s00 + 4U;
            const std::size_t s01 = s00 + static_cast<std::size_t>(cw) * 4U;
            const std::size_t s11 = s01 + 4U;
            const std::size_t p = surface_.offset(x, y);
            for (int k = 0; k < 4; ++k) {
                const float top =
                    scratch_[s00 + static_cast<std::size_t>(k)] +
                    (scratch_[s10 + static_cast<std::size_t>(k)] - scratch_[s00 + static_cast<std::size_t>(k)]) * fx;
                const float bottom =
                    scratch_[s01 + static_cast<std::size_t>(k)] +
                    (scratch_[s11 + static_cast<std::size_t>(k)] - scratch_[s01 + static_cast<std::size_t>(k)]) * fx;
                pixels[p + static_cast<std::size_t>(k)] =
                    static_cast<std::uint8_t>(std::clamp(std::lround(top + (bottom - top) * fy), 0L, 255L));
            }
        }
    }
}

namespace {

struct Matrix3 {
    double m[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
};

[[nodiscard]] Matrix3 square_to_quad(const Quad& c) {
    // Maps the unit square (0,0)->c0, (1,0)->c1, (1,1)->c2, (0,1)->c3, as the macOS app's
    // `DistortWarp.homography` does.
    const double sx = c.x[0] - c.x[1] + c.x[2] - c.x[3];
    const double sy = c.y[0] - c.y[1] + c.y[2] - c.y[3];
    double g = 0.0;
    double h = 0.0;
    if (std::abs(sx) > 1e-9 || std::abs(sy) > 1e-9) {
        const double dx1 = c.x[1] - c.x[2], dx2 = c.x[3] - c.x[2];
        const double dy1 = c.y[1] - c.y[2], dy2 = c.y[3] - c.y[2];
        const double den = dx1 * dy2 - dx2 * dy1;
        if (std::abs(den) > 1e-12) {
            g = (sx * dy2 - dx2 * sy) / den;
            h = (dx1 * sy - sx * dy1) / den;
        }
    }
    Matrix3 matrix;
    matrix.m[0][0] = c.x[1] - c.x[0] + g * c.x[1];
    matrix.m[0][1] = c.x[3] - c.x[0] + h * c.x[3];
    matrix.m[0][2] = c.x[0];
    matrix.m[1][0] = c.y[1] - c.y[0] + g * c.y[1];
    matrix.m[1][1] = c.y[3] - c.y[0] + h * c.y[3];
    matrix.m[1][2] = c.y[0];
    matrix.m[2][0] = g;
    matrix.m[2][1] = h;
    matrix.m[2][2] = 1.0;
    return matrix;
}

[[nodiscard]] Matrix3 invert(const Matrix3& matrix) {
    const double a = matrix.m[0][0], b = matrix.m[0][1], c = matrix.m[0][2];
    const double d = matrix.m[1][0], e = matrix.m[1][1], f = matrix.m[1][2];
    const double g = matrix.m[2][0], h = matrix.m[2][1], i = matrix.m[2][2];
    const double determinant = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    Matrix3 out;
    if (std::abs(determinant) < 1e-12) {
        return out;
    }
    const double inv = 1.0 / determinant;
    out.m[0][0] = (e * i - f * h) * inv;
    out.m[0][1] = (c * h - b * i) * inv;
    out.m[0][2] = (b * f - c * e) * inv;
    out.m[1][0] = (f * g - d * i) * inv;
    out.m[1][1] = (a * i - c * g) * inv;
    out.m[1][2] = (c * d - a * f) * inv;
    out.m[2][0] = (d * h - e * g) * inv;
    out.m[2][1] = (b * g - a * h) * inv;
    out.m[2][2] = (a * e - b * d) * inv;
    return out;
}

[[nodiscard]] std::uint8_t sample_channel(const RgbaSurface& image, double x, double y, int channel) {
    const double clamped_x = std::clamp(x, 0.0, static_cast<double>(image.width() - 1));
    const double clamped_y = std::clamp(y, 0.0, static_cast<double>(image.height() - 1));
    const int x0 = static_cast<int>(clamped_x);
    const int y0 = static_cast<int>(clamped_y);
    const int x1 = std::min(image.width() - 1, x0 + 1);
    const int y1 = std::min(image.height() - 1, y0 + 1);
    const double fx = clamped_x - x0;
    const double fy = clamped_y - y0;
    const auto at = [&](int px, int py) {
        return static_cast<double>(image.data()[image.offset(px, py) + static_cast<std::size_t>(channel)]);
    };
    const double top = at(x0, y0) + (at(x1, y0) - at(x0, y0)) * fx;
    const double bottom = at(x0, y1) + (at(x1, y1) - at(x0, y1)) * fx;
    return static_cast<std::uint8_t>(std::clamp(std::lround(top + (bottom - top) * fy), 0L, 255L));
}

}  // namespace

bool warp_perspective(const RgbaSurface& image, const Quad& corners, bool flip_x, bool flip_y, int& origin_x,
                      int& origin_y, RgbaSurface& out) {
    if (image.empty()) {
        return false;
    }
    double min_x = corners.x[0];
    double max_x = corners.x[0];
    double min_y = corners.y[0];
    double max_y = corners.y[0];
    for (int i = 1; i < 4; ++i) {
        min_x = std::min(min_x, corners.x[i]);
        max_x = std::max(max_x, corners.x[i]);
        min_y = std::min(min_y, corners.y[i]);
        max_y = std::max(max_y, corners.y[i]);
    }
    origin_x = static_cast<int>(std::floor(min_x));
    origin_y = static_cast<int>(std::floor(min_y));
    const int width = static_cast<int>(std::ceil(max_x)) - origin_x;
    const int height = static_cast<int>(std::ceil(max_y)) - origin_y;
    if (width < 1 || height < 1) {
        return false;
    }
    const Matrix3 inverse = invert(square_to_quad(corners));
    out = RgbaSurface(width, height);
    const double image_width = static_cast<double>(image.width());
    const double image_height = static_cast<double>(image.height());
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const double document_x = static_cast<double>(origin_x + x) + 0.5;
            const double document_y = static_cast<double>(origin_y + y) + 0.5;
            const double w = inverse.m[2][0] * document_x + inverse.m[2][1] * document_y + inverse.m[2][2];
            if (std::abs(w) < 1e-12) {
                continue;
            }
            double u = (inverse.m[0][0] * document_x + inverse.m[0][1] * document_y + inverse.m[0][2]) / w;
            double v = (inverse.m[1][0] * document_x + inverse.m[1][1] * document_y + inverse.m[1][2]) / w;
            if (u < 0.0 || u > 1.0 || v < 0.0 || v > 1.0) {
                continue;
            }
            if (flip_x) {
                u = 1.0 - u;
            }
            if (flip_y) {
                v = 1.0 - v;
            }
            const double sx = u * (image_width - 1.0);
            const double sy = v * (image_height - 1.0);
            out.set(x, y, sample_channel(image, sx, sy, 0), sample_channel(image, sx, sy, 1),
                    sample_channel(image, sx, sy, 2), sample_channel(image, sx, sy, 3));
        }
    }
    return true;
}

}  // namespace compositor::render

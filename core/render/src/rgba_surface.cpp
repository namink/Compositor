#include "compositor/render/rgba_surface.hpp"

#include <algorithm>
#include <cmath>

namespace compositor::render {
namespace {

[[nodiscard]] std::uint8_t to_byte(float value) {
    const float clamped = std::clamp(value, 0.0F, 1.0F);
    return static_cast<std::uint8_t>(std::lround(clamped * 255.0F));
}

}  // namespace

RgbaSurface::RgbaSurface(int width, int height)
    : width_(std::max(0, width)),
      height_(std::max(0, height)),
      pixels_(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * 4U, 0U) {}

void RgbaSurface::set(int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    const std::size_t index = offset(x, y);
    pixels_[index] = r;
    pixels_[index + 1] = g;
    pixels_[index + 2] = b;
    pixels_[index + 3] = a;
}

void unpremultiply(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a, RgbF& color, float& alpha) {
    alpha = static_cast<float>(a) / 255.0F;
    if (a == 0) {
        color = RgbF{};
        return;
    }
    const float scale = 255.0F / static_cast<float>(a);
    color.r = std::min(1.0F, static_cast<float>(r) * scale / 255.0F);
    color.g = std::min(1.0F, static_cast<float>(g) * scale / 255.0F);
    color.b = std::min(1.0F, static_cast<float>(b) * scale / 255.0F);
}

void premultiply(const RgbF& color, float alpha, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b, std::uint8_t& a) {
    const float clamped_alpha = std::clamp(alpha, 0.0F, 1.0F);
    r = to_byte(color.r * clamped_alpha);
    g = to_byte(color.g * clamped_alpha);
    b = to_byte(color.b * clamped_alpha);
    a = to_byte(clamped_alpha);
}

}  // namespace compositor::render

#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace compositor::render {

/// An unpremultiplied RGB color with components in [0, 1].
struct RgbF {
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
};

/// A premultiplied 8-bit RGBA image, sRGB, top row first — the same layout the `.comp` layer PNGs
/// decode to and the macOS canvas composites in. Storing premultiplied lets the compositor add
/// coverage without un-premultiplying every pixel it touches.
class RgbaSurface {
public:
    RgbaSurface() = default;
    RgbaSurface(int width, int height);

    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }
    [[nodiscard]] bool empty() const { return pixels_.empty(); }

    [[nodiscard]] const std::uint8_t* data() const { return pixels_.data(); }
    [[nodiscard]] std::uint8_t* data() { return pixels_.data(); }

    /// Byte offset of pixel (x, y); no bounds check.
    [[nodiscard]] std::size_t offset(int x, int y) const {
        return (static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)) * 4U;
    }

    /// Set a pixel from premultiplied components (already rounded to 0..255).
    void set(int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a);

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<std::uint8_t> pixels_;
};

/// Split a premultiplied pixel into unpremultiplied color and alpha. A fully transparent pixel
/// reports black, matching how the macOS app reads transparent pixels.
void unpremultiply(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a, RgbF& color, float& alpha);

/// Round an unpremultiplied color and alpha back to a premultiplied 8-bit pixel.
void premultiply(const RgbF& color, float alpha, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b, std::uint8_t& a);

}  // namespace compositor::render

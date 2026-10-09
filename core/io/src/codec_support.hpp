#pragma once
#include <cstdint>
#include <vector>

#include "compositor/render/rgba_surface.hpp"

namespace compositor::io::detail {

/// Straight (non-premultiplied) channel times alpha, rounded, as an 8-bit premultiplied value.
[[nodiscard]] inline std::uint8_t premultiply_byte(std::uint8_t channel, std::uint8_t alpha) {
    return static_cast<std::uint8_t>((static_cast<unsigned>(channel) * alpha + 127U) / 255U);
}

/// Build a premultiplied surface from straight RGBA bytes (row-major, top row first).
[[nodiscard]] inline compositor::render::RgbaSurface from_straight_rgba(const std::uint8_t* rgba, int width,
                                                                        int height) {
    compositor::render::RgbaSurface surface(width, height);
    std::uint8_t* out = surface.data();
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t at = i * 4U;
        const std::uint8_t alpha = rgba[at + 3];
        out[at] = premultiply_byte(rgba[at], alpha);
        out[at + 1] = premultiply_byte(rgba[at + 1], alpha);
        out[at + 2] = premultiply_byte(rgba[at + 2], alpha);
        out[at + 3] = alpha;
    }
    return surface;
}

/// Unpremultiply a surface back to straight RGBA bytes, for encoders that expect straight alpha.
[[nodiscard]] inline std::vector<std::uint8_t> to_straight_rgba(const compositor::render::RgbaSurface& surface) {
    std::vector<std::uint8_t> rgba(surface.data(),
                                   surface.data() + surface.offset(surface.width() - 1, surface.height() - 1) + 4U);
    const std::size_t count = static_cast<std::size_t>(surface.width()) * static_cast<std::size_t>(surface.height());
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t at = i * 4U;
        const std::uint8_t alpha = rgba[at + 3];
        if (alpha == 0) {
            rgba[at] = rgba[at + 1] = rgba[at + 2] = 0;
            continue;
        }
        for (std::size_t c = 0; c < 3U; ++c) {
            const unsigned value = (static_cast<unsigned>(rgba[at + c]) * 255U + alpha / 2U) / alpha;
            rgba[at + c] = static_cast<std::uint8_t>(value > 255U ? 255U : value);
        }
    }
    return rgba;
}

}  // namespace compositor::io::detail

#include "compositor/model/png_header.hpp"

#include <array>

namespace compositor::model {
namespace {

constexpr std::array<std::uint8_t, 8> kSignature{0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

[[nodiscard]] std::uint32_t read_u32_be(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(bytes[offset]) << 24) | (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) | static_cast<std::uint32_t>(bytes[offset + 3]);
}

}  // namespace

PngInfo parse_png_header(std::span<const std::uint8_t> bytes) {
    PngInfo info;
    // Signature (8) + IHDR length/type (8) + width/height (8) + depth (1) + color type (1).
    if (bytes.size() < 26) {
        return info;
    }
    for (std::size_t i = 0; i < kSignature.size(); ++i) {
        if (bytes[i] != kSignature[i]) {
            return info;
        }
    }
    if (bytes[12] != 'I' || bytes[13] != 'H' || bytes[14] != 'D' || bytes[15] != 'R') {
        return info;
    }
    info.is_png = true;
    info.width = static_cast<int>(read_u32_be(bytes, 16));
    info.height = static_cast<int>(read_u32_be(bytes, 20));
    info.bit_depth = bytes[24];
    info.color_type = bytes[25];
    return info;
}

std::string describe(const PngInfo& info) {
    if (!info.is_png) {
        return "not a PNG";
    }
    return "PNG " + std::to_string(info.width) + "x" + std::to_string(info.height) + ", depth " +
           std::to_string(info.bit_depth) + ", type " + std::to_string(info.color_type);
}

}  // namespace compositor::model

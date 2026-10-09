#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace compositor::model {

/// The fields of interest from a PNG's IHDR chunk.
struct PngInfo {
    bool is_png = false;
    int width = 0;
    int height = 0;
    int bit_depth = 0;
    int color_type = 0;
};

/// Read the signature and IHDR of a PNG. Returns `is_png == false` when the bytes are not a PNG or
/// the header is truncated; never throws. Only the first 33 bytes are inspected, so a multi-hundred
/// megabyte asset is not copied to learn its size.
[[nodiscard]] PngInfo parse_png_header(std::span<const std::uint8_t> bytes);

/// A short human-readable summary for error messages, for example "PNG 800x600, depth 8, type 6".
[[nodiscard]] std::string describe(const PngInfo& info);

}  // namespace compositor::model

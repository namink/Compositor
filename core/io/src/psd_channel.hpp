#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace compositor::io::detail {

/// Unpack one 8-bit channel plane (compression 0 raw, 1 PackBits) to `width * height` bytes.
[[nodiscard]] std::vector<std::uint8_t> decode_channel(int compression, int width, int height,
                                                       std::span<const std::uint8_t> data, bool large_document);

}  // namespace compositor::io::detail

#include "psd_channel.hpp"

#include <cstddef>

#include "compositor/io/psd_reader.hpp"

namespace compositor::io::detail {
namespace {

[[nodiscard]] std::vector<std::uint8_t> unpack_rle(int width, int height, std::span<const std::uint8_t> data,
                                                   bool large_document) {
    std::size_t offset = 0;
    const auto next = [&](void) -> std::uint8_t {
        if (offset >= data.size()) {
            throw PsdError("The Photoshop layer data is truncated.");
        }
        return data[offset++];
    };
    std::vector<std::size_t> counts(static_cast<std::size_t>(height));
    for (int row = 0; row < height; ++row) {
        if (large_document) {
            const std::uint32_t value = (static_cast<std::uint32_t>(next()) << 24) |
                                        (static_cast<std::uint32_t>(next()) << 16) |
                                        (static_cast<std::uint32_t>(next()) << 8) | next();
            counts[static_cast<std::size_t>(row)] = value;
        } else {
            counts[static_cast<std::size_t>(row)] = (static_cast<std::size_t>(next()) << 8) | next();
        }
    }
    std::vector<std::uint8_t> plane(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0U);
    for (int row = 0; row < height; ++row) {
        const std::size_t end = offset + counts[static_cast<std::size_t>(row)];
        if (end > data.size()) {
            throw PsdError("The Photoshop layer data is truncated.");
        }
        int written = 0;
        while (written < width) {
            if (offset >= end) {
                throw PsdError("The Photoshop layer data is truncated.");
            }
            const auto control = static_cast<std::int8_t>(data[offset++]);
            if (control >= 0) {
                const int count = control + 1;
                if (written + count > width || offset + static_cast<std::size_t>(count) > end) {
                    throw PsdError("The Photoshop layer data is truncated.");
                }
                for (int i = 0; i < count; ++i) {
                    plane[static_cast<std::size_t>(row) * width + written + i] =
                        data[offset + static_cast<std::size_t>(i)];
                }
                offset += static_cast<std::size_t>(count);
                written += count;
            } else if (control != -128) {
                const int count = 1 - control;
                if (written + count > width || offset >= end) {
                    throw PsdError("The Photoshop layer data is truncated.");
                }
                const std::uint8_t value = data[offset++];
                for (int i = 0; i < count; ++i) {
                    plane[static_cast<std::size_t>(row) * width + written + i] = value;
                }
                written += count;
            }
        }
        offset = end;
    }
    return plane;
}

}  // namespace

std::vector<std::uint8_t> decode_channel(int compression, int width, int height, std::span<const std::uint8_t> data,
                                         bool large_document) {
    if (width <= 0 || height <= 0) {
        return {};
    }
    const std::size_t expected = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (compression == 0) {
        if (data.size() < expected) {
            throw PsdError("The Photoshop layer data is truncated.");
        }
        return std::vector<std::uint8_t>(data.begin(), data.begin() + static_cast<std::ptrdiff_t>(expected));
    }
    if (compression == 1) {
        return unpack_rle(width, height, data, large_document);
    }
    throw PsdError("This Photoshop file uses an unsupported layer compression method.");
}

}  // namespace compositor::io::detail

#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace comp_test {

/// A tiny PNG encoder for tests, so fixtures need no image library. It writes 8-bit images using
/// stored (uncompressed) deflate blocks: valid PNG, deterministic bytes, easy to reason about.

inline std::uint32_t crc32(std::span<const std::uint8_t> data) {
    static const std::uint32_t* table = [] {
        static std::uint32_t values[256];
        for (std::uint32_t n = 0; n < 256; ++n) {
            std::uint32_t c = n;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1U) != 0U ? 0xEDB88320U ^ (c >> 1) : c >> 1;
            }
            values[n] = c;
        }
        return values;
    }();
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const std::uint8_t byte : data) {
        crc = table[(crc ^ byte) & 0xFFU] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFU;
}

inline std::uint32_t adler32(std::span<const std::uint8_t> data) {
    std::uint32_t a = 1;
    std::uint32_t b = 0;
    for (const std::uint8_t byte : data) {
        a = (a + byte) % 65521U;
        b = (b + a) % 65521U;
    }
    return (b << 16) | a;
}

inline void push_u32_be(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value >> 24));
    out.push_back(static_cast<std::uint8_t>(value >> 16));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
    out.push_back(static_cast<std::uint8_t>(value));
}

inline void append_chunk(std::vector<std::uint8_t>& out, const char type[4], std::span<const std::uint8_t> data) {
    push_u32_be(out, static_cast<std::uint32_t>(data.size()));
    const std::size_t crc_start = out.size();
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::uint8_t>(type[i]));
    }
    out.insert(out.end(), data.begin(), data.end());
    const std::uint32_t crc = crc32(std::span<const std::uint8_t>(out).subspan(crc_start));
    push_u32_be(out, crc);
}

inline std::vector<std::uint8_t> zlib_stored(std::span<const std::uint8_t> raw) {
    std::vector<std::uint8_t> out{0x78, 0x01};
    std::size_t offset = 0;
    do {
        const std::size_t remaining = raw.size() - offset;
        const std::size_t n = remaining > 65535 ? 65535 : remaining;
        const bool final_block = offset + n >= raw.size();
        out.push_back(final_block ? 1 : 0);  // BFINAL, BTYPE = 00 (stored)
        out.push_back(static_cast<std::uint8_t>(n & 0xFF));
        out.push_back(static_cast<std::uint8_t>((n >> 8) & 0xFF));
        const std::uint16_t nlen = static_cast<std::uint16_t>(~n);
        out.push_back(static_cast<std::uint8_t>(nlen & 0xFF));
        out.push_back(static_cast<std::uint8_t>((nlen >> 8) & 0xFF));
        out.insert(out.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset),
                   raw.begin() + static_cast<std::ptrdiff_t>(offset + n));
        offset += n;
    } while (offset < raw.size());
    const std::uint32_t check = adler32(raw);
    push_u32_be(out, check);
    return out;
}

/// A PNG of `width` x `height`, 8-bit, filled with `fill`, of the given PNG color type
/// (0 = gray, 2 = RGB, 4 = gray+alpha, 6 = RGBA).
inline std::vector<std::uint8_t> make_png(std::uint32_t width, std::uint32_t height, int color_type,
                                          std::uint8_t fill = 255) {
    const int channels = color_type == 6 ? 4 : color_type == 2 ? 3 : color_type == 4 ? 2 : 1;
    std::vector<std::uint8_t> raw;
    raw.reserve(static_cast<std::size_t>(height) * (1 + width * channels));
    for (std::uint32_t y = 0; y < height; ++y) {
        raw.push_back(0);  // filter: none
        for (std::uint32_t x = 0; x < width; ++x) {
            for (int c = 0; c < channels; ++c) {
                raw.push_back(fill);
            }
        }
    }

    std::vector<std::uint8_t> ihdr;
    push_u32_be(ihdr, width);
    push_u32_be(ihdr, height);
    ihdr.push_back(8);  // bit depth
    ihdr.push_back(static_cast<std::uint8_t>(color_type));
    ihdr.push_back(0);  // compression
    ihdr.push_back(0);  // filter
    ihdr.push_back(0);  // interlace

    std::vector<std::uint8_t> out{0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    append_chunk(out, "IHDR", ihdr);
    const std::vector<std::uint8_t> idat = zlib_stored(raw);
    append_chunk(out, "IDAT", idat);
    append_chunk(out, "IEND", {});
    return out;
}

}  // namespace comp_test

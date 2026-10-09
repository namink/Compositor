#pragma once
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "compositor/render/rgba_surface.hpp"

namespace compositor::io {

/// Raised when bytes are not a readable image, or an image cannot be encoded.
class ImageCodecError : public std::runtime_error {
public:
    explicit ImageCodecError(const std::string& message) : std::runtime_error(message) {}
};

/// Quick format checks that read only a few leading bytes.
[[nodiscard]] bool looks_like_png(std::span<const std::uint8_t> bytes);
[[nodiscard]] bool looks_like_jpeg(std::span<const std::uint8_t> bytes);

/// Decode to a premultiplied sRGB surface, the form the compositor works in. 8-bit only; 16-bit
/// images are refused rather than silently reduced. An alpha-less image comes back fully opaque.
[[nodiscard]] compositor::render::RgbaSurface decode_png(std::span<const std::uint8_t> bytes);
[[nodiscard]] compositor::render::RgbaSurface decode_jpeg(std::span<const std::uint8_t> bytes);

/// Dispatch on the leading bytes: PNG or JPEG, else `ImageCodecError`.
[[nodiscard]] compositor::render::RgbaSurface decode_image(std::span<const std::uint8_t> bytes);

/// RAW camera files (LibRaw). True when this build was compiled with LibRaw.
[[nodiscard]] bool raw_supported();
/// Whether `extension` (lower-case, with or without the dot) is a common camera-raw format.
[[nodiscard]] bool is_raw_extension(const std::string& extension);
/// Decode a camera-raw file (or in-memory bytes) through LibRaw's demosaic and camera white balance,
/// returning a straight-sRGB premultiplied surface (opaque). Throws `ImageCodecError` when unsupported
/// or when the build has no LibRaw.
[[nodiscard]] compositor::render::RgbaSurface decode_raw(std::span<const std::uint8_t> bytes);
[[nodiscard]] compositor::render::RgbaSurface decode_raw_file(const std::string& path);

/// Encode straight-alpha sRGB output. The surface is unpremultiplied first; JPEG has no alpha and
/// composites on white. Used by tests and, later, by export.
[[nodiscard]] std::vector<std::uint8_t> encode_png(const compositor::render::RgbaSurface& surface);
[[nodiscard]] std::vector<std::uint8_t> encode_jpeg(const compositor::render::RgbaSurface& surface, int quality);

}  // namespace compositor::io
